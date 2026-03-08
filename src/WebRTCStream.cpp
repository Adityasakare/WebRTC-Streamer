#include "../include/WebRTCStream.h"


WebRTCStream::WebRTCStream(const std::string& devicePath,
                 const std::string& displayName,
                 const std::string& userName,
                 SoupWebsocketConnection* wsConn,
                 GMainLoop* loop)
    :m_devicePath(devicePath),
    m_displayName(displayName),
    m_userName(userName),
    m_pendingClientId(""),
    m_wsConn(wsConn),
    m_loop(loop),
    m_pipeline(nullptr),
    m_webrtcbin(nullptr),
    m_pipelineReady(false)
{
    Logger::getInstance().log(LogLevel::INFO, "[%s] Stream created - waiting for viewer", m_displayName.c_str());
}

WebRTCStream::~WebRTCStream()
{
    stop();
}


void WebRTCStream::stop(void)
{
    if(!m_pipeline)
        return;

    // Send EOS so splitmuxsink finalize the current recording chunk
    if(m_pipelineReady)
    {
        Logger::getInstance().log(LogLevel::INFO, "[%s] Sending EOS - finalizing recording...", m_displayName.c_str());

        gst_element_send_event(m_pipeline, gst_event_new_eos());

        GstBus* bus = gst_pipeline_get_bus(GST_PIPELINE(m_pipeline));
        GstMessage* msg = gst_bus_timed_pop_filtered(bus, 5 * GST_SECOND,
                    (GstMessageType)(GST_MESSAGE_EOS | GST_MESSAGE_ERROR));

        if(msg)
        {
            gst_message_unref(msg);
        }
        gst_object_unref(bus);
    }

    gst_element_set_state(m_pipeline, GST_STATE_NULL);
    if(m_webrtcbin)
    {
        gst_object_unref(m_webrtcbin);
        m_webrtcbin = nullptr;
    }

    gst_object_unref(m_pipeline);
    
    m_pipeline = nullptr;
    m_pipelineReady = false;
    m_pendingClientId.clear();
}

void WebRTCStream::buildPipeline(void)
{
    // Extract device id from path: /dev/video0 -> video0
    std::string devId = m_devicePath;
    size_t slash = devId.rfind('/');
    if(slash != std::string::npos)
        devId = devId.substr(slash + 1);

    std::string webrtcName = "webrtcbin_" + devId;
    std::string payName = "pay_" + devId;
    std::string teeName = "tee_" + devId;

    // Recording file pattern
    g_mkdir_with_parents(RECORDING_DIR, 0755);
    time_t now = time(nullptr);
    char ts[32];
    strftime(ts, sizeof(ts), "%Y%M%d_%H%M%S", localtime(&now));
    char recPath[256];
    snprintf(recPath, sizeof(recPath), "%s/%s_%s_%%05d.mp4", RECORDING_DIR, devId.c_str(), ts);


    char desc[4056];
        snprintf(desc, sizeof(desc),
        " webrtcbin name=%s bundle-policy=max-bundle stun-server=%s latency=100 "
        " v4l2src device=%s do-timestamp=true ! "
        " videoconvert ! videoscale ! "
        " video/x-raw,width=%d,height=%d,framerate=%d/1 ! "
        " videoconvert ! "
        " clockoverlay valignment=bottom halignment=left "
            " font-desc=\"Sans Bold 18\" "
            " time-format=\"%%Y-%%m-%%d  %%H:%%M:%%S\" "
            " shading-value=80 ! "
        " x264enc bitrate=%d speed-preset=ultrafast tune=zerolatency key-int-max=%d ! "
        " video/x-h264,profile=baseline ! "
        " h264parse config-interval=-1 ! "
        " tee name=%s "
        " %s. ! queue max-size-buffers=10 leaky=downstream ! "
        " rtph264pay config-interval=-1 pt=%d aggregate-mode=zero-latency name=%s "
        " %s. ! queue max-size-buffers=300 leaky=downstream ! "
        " splitmuxsink location=%s max-size-time=%" G_GUINT64_FORMAT
        " muxer-factory=mp4mux send-keyframe-requests=false async-finalize=true ",
        webrtcName.c_str(), STUN_SERVER,
        m_devicePath.c_str(),
        VIDEO_WIDTH, VIDEO_HEIGHT, VIDEO_FRAMERATE,
        H264_BITRATE_KBPS, H264_KEY_INT_MAX,
        teeName.c_str(),
        teeName.c_str(), RTP_PAYLOAD_TYPE, payName.c_str(),
        teeName.c_str(), recPath,
        (guint64)RECORDING_CHUNKS_SECS * GST_SECOND);

        GError* err = nullptr;
        m_pipeline = gst_parse_launch(desc, &err);
        if(err)
        {
            Logger::getInstance().log(LogLevel::ERROR, "[%s] Pipeline parse failed: %s",
                                        m_displayName.c_str(), err->message);
            g_error_free(err);
            return;
        }

        m_webrtcbin = gst_bin_get_by_name(GST_BIN(m_pipeline), webrtcName.c_str());
        if(!m_webrtcbin)
        {
            Logger::getInstance().log(LogLevel::ERROR, "[%s] webrtcbin not found",
                                        m_displayName.c_str());
            gst_object_unref(m_pipeline);
            m_pipeline = nullptr;
            return;
        }

        // BUs watch
        GArray* trans = nullptr;
        g_signal_emit_by_name(m_webrtcbin, "get-transceivers", &trans);
        if(trans && trans->len > 0)
        {
            GstWebRTCRTPTransceiver* t = g_array_index(trans, GstWebRTCRTPTransceiver*, 0);
            g_object_set(t, "direction", GST_WEBRTC_RTP_TRANSCEIVER_DIRECTION_SENDONLY, nullptr);
            g_array_unref(trans);
        }

        g_signal_connect(m_webrtcbin, "on-negotiation-needed", G_CALLBACK(onNegotiationNeeded_s), this);
        g_signal_connect(m_webrtcbin, "on-ice-candidate", G_CALLBACK(onIceCandidate_s), this);

        GstStateChangeReturn sc = gst_element_set_state(m_pipeline, GST_STATE_PLAYING);

        if(sc == GST_STATE_CHANGE_FAILURE)
        {
            Logger::getInstance().log(LogLevel::ERROR, "[%s] Pipeline failed to reach PLAYING", m_devicePath.c_str());
            return;
        }

        m_pipelineReady = true;
        Logger::getInstance().log(LogLevel::INFO, "[%s] Pipeline PLAYING - Recording to %s", m_displayName.c_str(), recPath);
}


void WebRTCStream::linkPayloaderToWebrtcbin(const std::string& devId, const std::string& payName)
{
    GstElement* pay = gst_bin_get_by_name(GST_BIN(m_pipeline), payName.c_str());
    if(!pay)
    {
        Logger::getInstance().log(LogLevel::ERROR, "[%s] payloader not found", m_displayName.c_str());
        return;
    }

    GstPad* src = gst_element_get_static_pad(pay, "src");
    GstPad* sink = gst_element_request_pad_simple(m_webrtcbin, "sink_%u");

    if(src && sink)
    {
        if(gst_pad_link(src, sink) == GST_PAD_LINK_OK)
            Logger::getInstance().log(LogLevel::INFO, "[%s] payloader linked to webrtcbin", m_displayName.c_str());
        else
            Logger::getInstance().log(LogLevel::ERROR, "[%s] pad link failed", m_displayName.c_str());
    }
    
    if(src)
        gst_object_unref(src);

    if(sink)
        gst_object_unref(sink);
    gst_object_unref(pay);
}

void WebRTCStream::onNegotiationNeeded(void)
{
    Logger::getInstance().log(LogLevel::INFO, "[%s] Negotiation needed - creating offer for client=%s", m_displayName.c_str(), 
                            m_pendingClientId.c_str());

    GstPromise* p = gst_promise_new_with_change_func(onOfferCreated_s, this, nullptr);
    g_signal_emit_by_name(m_webrtcbin, "create-oofer", nullptr, p);
}

void WebRTCStream::onOfferCreated(GstPromise* promise)
{
    GstWebRTCSessionDescription* offer = nullptr;
    const GstStructure* reply = gst_promise_get_reply(promise);
    gst_structure_get(reply, "offer", GST_TYPE_WEBRTC_SESSION_DESCRIPTION, &offer, nullptr);
    gst_promise_unref(promise);

    GstPromise* local = gst_promise_new();
    g_signal_emit_by_name(m_webrtcbin, "set-local-description", offer, local);
    gst_promise_interrupt(local);
    gst_promise_unref(local);

    gchar* sdpStr = gst_sdp_message_as_text(offer->sdp);

    JsonObject* dataObj = json_object_new();
    json_object_set_string_member(dataObj, "type", "offer");
    json_object_set_string_member(dataObj, "sdp", sdpStr);

    JsonObject* root = json_object_new();
    json_object_set_string_member(root, "type", "streamer:offer");
    json_object_set_string_member(root, "device", m_devicePath.c_str());
    json_object_set_string_member(root, "username", m_userName.c_str());
    json_object_set_string_member(root, "clientId", m_pendingClientId.c_str());
    json_object_set_object_member(root, "data", dataObj);

    SendJson(root);
    g_free(sdpStr);
    gst_webrtc_session_description_free(offer);

    Logger::getInstance().log(LogLevel::INFO, "[%s] Offer sent to client=%s", m_displayName.c_str(), m_pendingClientId.c_str());
}


void WebRTCStream::handleMessage(const std::string& json)
{
    JsonParser* parser = json_parser_new();
    if(!json_parser_load_from_data(parser, json.c_str(), -1, nullptr))
    {
        g_object_unref(parser);
        return;
    }

    JsonObject* root = json_node_get_object(json_parser_get_root(parser));

    // copy all values out before freeing parser
    std::string type;
    std::string clientId;
    std::string sdp;
    std::string iceCandidate;
    gint64 sdpMLineIndex = 0;

    if(json_object_has_member(root, "type"))
        type = json_object_get_string_member(root, "type");

    if(json_object_has_member(root, "clientId"))
    {
        gint64 id = json_object_get_int_member(root, "clientId");
        char buf[32];
        snprintf(buf, sizeof(buf), "%", G_GINT64_FORMAT, id);
        clientId = buf;
    }

     if(json_object_has_member(root, "data"))
     {
        JsonObject* data = json_object_get_object_member(root, "data");
        if(data)
        {
            if(json_object_has_member(data, "sdp"))
                sdp = json_object_get_string_member(data, "sdp");
            if(json_object_has_member(data, "candidate"))
                iceCandidate = json_object_get_string_member(data, "candidate");
            if(json_object_has_member(data, "sdpMLineIndex"))
                sdpMLineIndex = json_object_get_int_member(data, "sdpMLineIndex");
        }
     }

    g_object_unref(parser);
    
    // Dispatch
    if(type =="signal")
    {
        if(!clientId.empty())
            m_pendingClientId = clientId;
        if(!m_pipelineReady)
            buildPipeline();
        else
        {
            GstPromise* p = gst_promise_new_with_change_func(onOfferCreated_s, this, nullptr);
            g_signal_emit_by_name(m_webrtcbin, "create-offer", nullptr, p);
        }
    }
    else if(type == "sdp")
    {
        GstSDPMessage* sdpMsg = nullptr;
        gst_sdp_message_new(&sdpMsg);
        gst_sdp_message_parse_buffer((guint8*)sdp.c_str(), sdp.size(), sdpMsg);

        GstWebRTCSessionDescription* answer = gst_webrtc_session_description_new(GST_WEBRTC_SDP_TYPE_ANSWER, sdpMsg);
        GstPromise* p = gst_promise_new();
        g_signal_emit_by_name(m_webrtcbin, "set-remote-description", answer, p);
        gst_promise_interrupt(p);
        gst_promise_unref(p);
        gst_webrtc_session_description_free(answer);
        Logger::getInstance().log(LogLevel::INFO, "[%s] SDP answer applied", m_displayName.c_str());
    }
    else if(type == "ice")
    {
        if(!iceCandidate.empty())
            g_signal_emit_by_name(m_webrtcbin, "add-ice-candidate", (guint)sdpMLineIndex, iceCandidate.c_str());
    }

        
}
