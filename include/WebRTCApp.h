#ifndef WEBRTCAPP_H
#define WEBRTCAPP_H

#ifndef GST_USE_UNSTABLE_API
#define GST_USE_UNSTABLE_API
#endif

#include <glib.h>
#include <libsoup/soup.h>
#include <json-glib/json-glib.h>
#include <string>
#include <map>

#include "Logger.h"
#include "Config.h"
#include "CameraConfig.h"


class WebRTCStream;

class WebRTCApp
{
private:
    std::string              m_serverUrl;
    std::string              m_userName;
    int                      m_reconnectCount;

    GMainLoop*               m_loop;
    SoupWebsocketConnection* m_wsConn;

    CameraConfig                         m_config;
    std::map<std::string, WebRTCStream*> m_streams;

    void connectToServer();
    void registerWithServer();
    void scheduleReconnect();

    void onServerConnected(SoupSession*, GAsyncResult*);
    void onWebsocketClosed();
    void onWebsocketMessage(SoupWebsocketConnection*,SoupWebsocketDataType, GBytes*);
    static void onServerConnected_s(SoupSession*, GAsyncResult*, gpointer);
    static void onWebsocketClosed_s(SoupWebsocketConnection*, gpointer);
    static void onWebsocketMessage_s(SoupWebsocketConnection*,SoupWebsocketDataType, GBytes*, gpointer);
    

public:
    WebRTCApp(const std::string& confFile,
              const std::string& serverUrl,
              const std::string& userName);
    ~WebRTCApp();

    WebRTCApp(const WebRTCApp&)            = delete;
    WebRTCApp& operator=(const WebRTCApp&) = delete;

    void run(void);
    static gboolean reconnectCb(gpointer userData);
    void quit();
};


#endif
