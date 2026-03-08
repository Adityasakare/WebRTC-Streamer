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





