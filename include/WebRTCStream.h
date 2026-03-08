#ifndef WEBRTCSTREAM_H
#define WEBRTCSTREAM_H

#ifndef GST_USE_UNSTABLE_API
#define GST_USE_UNSTABLE_API
#endif

#include <gst/gst.h>
#include <gst/sdp/sdp.h>
#include <gst/webrtc/webrtc.h>
#include <libsoup/soup.h>
#include <json-glib/json-glib.h>
#include <string>

#include "Logger.h"
#include "Config.h"


class WebRTCStream
{
public:
    WebRTCStream(const std::string& devicePath,
                 const std::string& displayName,
                 const std::string& userName,
                 SoupWebsocketConnection* wsConn,
                 GMainLoop* loop);
    ~WebRTCStream();

    WebRTCStream(const WebRTCStream&)             = delete;
    WebRTCStream& operator=(const ~WebRTCStream&) = delete;

    void stop();
    void handleMessage(const std::string& json);

private:
    std::string             m_devicePath;
    std::string             m_displayName;
    std::string             m_userName;
    std::string             m_pendingClientId;
    
    SoupWebsocketConnection* m_wsConn;
    GMainLoop*              m_loop;
    
    GstElement*             m_pipeline;
    GstElement*             m_webrtcbin;

    bool                    m_pipelineReady;
};


#endif
