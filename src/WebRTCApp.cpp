#include "../include/WebRTCApp.h"
#include "../include/WebRTCStream.h"

WebRTCApp::WebRTCApp(const std::string& confFile,
              const std::string& serverUrl,
              const std::string& userName)
    : m_serverUrl(serverUrl),
      m_userName(userName),
      m_reconnectCount(0),
      m_loop(nullptr),
      m_wsConn(nullptr)
{
    m_config.load(confFile);
}



WebRTCApp::~WebRTCApp()
{
    for(auto& kv : m_streams)
    {
        kv.second->stop();
        delete kv.second;
    }
    m_streams.clear();

    if(m_wsConn)
    {
        g_object_unref(m_wsConn);
        m_wsConn = nullptr;
    }
    if(m_loop)
    {
        g_main_loop_unref(m_loop);
        m_loop = nullptr;
    }
}

void WebRTCApp::run(void)
{
    m_loop = g_main_loop_new(nullptr, FALSE);
    connectToServer();
    Logger::getInstance().log(LogLevel::INFO, "WebRTCApp: running with %zu cameras", m_config.size());
    g_main_loop_run(m_loop);
}

void WebRTCApp::connectToServer(void)
{
    Logger::getInstance().log(LogLevel::INFO, "WebRTCApp: connecting to %s", m_serverUrl.c_str());

    SoupSession* session = soup_session_new();
    SoupMessage* msg = soup_message_new(SOUP_METHOD_GET, m_serverUrl.c_str());

    soup_session_websocket_connect_async(session, msg, nullptr, nullptr, G_PRIORITY_DEFAULT, nullptr,
                                    reinterpret_cast<GAsyncReadyCallback>(onServerConnected_s), this);
}


void WebRTCApp::registerWithServer(void)
{
    for(auto& cam : m_config.Cameras())
    {
        JsonObject* root = json_object_new();
        json_object_set_string_member(root, "type", "streamer:register");
        json_object_set_string_member(root, "username", m_userName.c_str());
        json_object_set_string_member(root, "device", cam.devicePath.c_str());

        JsonNode* node = json_node_init_object(json_node_alloc(), root);
        JsonGenerator* gen = json_generator_new();
        json_generator_set_root(gen, node);
        gchar* text = json_generator_to_data(gen, nullptr);
        soup_websocket_connection_send_text(m_wsConn, text);
        g_free(text);
        g_object_unref(gen);
        json_node_free(node);

        Logger::getInstance().log(LogLevel::INFO, "WebRTCApp: registred %s", cam.devicePath.c_str());

        if(m_streams.find(cam.devicePath) == m_streams.end())
        {
            m_streams[cam.devicePath] = new WebRTCStream(
                cam.devicePath, cam.displayName,
                m_userName, m_wsConn, m_loop);
        }
    }
}

void WebRTCApp::onWebsocketMessage(SoupWebsocketConnection*,SoupWebsocketDataType dtype, GBytes* message)
{
    if(dtype == SOUP_WEBSOCKET_DATA_BINARY)
    {
        g_bytes_unref(message);
        return;
    }

    gsize size;
    gconstpointer raw = g_bytes_get_data(message, &size);
    std::string json(static_cast<const char*>(raw), size);
    g_bytes_unref(message);

    JsonParser* parser = json_parser_new();
    if(!json_parser_load_from_data(parser, json.c_str(), -1, nullptr))
    {
        g_object_unref(parser);
        return;
    }

    JsonObject* root = json_node_get_object(json_parser_get_root(parser));

    std::string type;
    std::string dev;

    if(json_object_has_member(root, "type"))
        type = json_object_get_string_member(root, "type");
    if(json_object_has_member(root, "device"))
        dev = json_object_get_string_member(root, "device");

    g_object_unref(parser);

    if(type == "server:registered")
    {
        Logger::getInstance().log(LogLevel::INFO, "WebRTCApp: server acknowledge registration");
        return;
    }

    if(!dev.empty())
    {
        auto it = m_streams.find(dev);
        if(it != m_streams.end())
            it->second->handleMessage(json);
    }
}

void WebRTCApp::onServerConnected(SoupSession* session, GAsyncResult* res)
{
    GError* err = nullptr;
    m_wsConn = soup_session_websocket_connect_finish(session, res, &err);
    if(err)
    {
        Logger::getInstance().log(LogLevel::ERROR, "WebRTCApp: connect failed: %s", err->message);
        g_error_free(err);
        scheduleReconnect();
        return;
    }

    Logger::getInstance().log(LogLevel::INFO, "WebRTCApp: connected to signaling server");
    m_reconnectCount = 0;

    g_signal_connect(m_wsConn, "closed", G_CALLBACK(onWebsocketClosed_s), this);
    g_signal_connect(m_wsConn, "message", G_CALLBACK(onWebsocketMessage_s), this);

    registerWithServer();
}


void WebRTCApp::onWebsocketClosed(void)
{
    Logger::getInstance().log(LogLevel::WARNING, "WebRTCApp: connection closed");

    if(m_wsConn)
    {
        g_object_unref(m_wsConn);
        m_wsConn = nullptr;
    }
    for(auto& kv : m_streams)
        kv.second->stop();
    
    scheduleReconnect();
}

void WebRTCApp::scheduleReconnect(void)
{
    ++m_reconnectCount;
    int delay = RECONNECT_DELAY_MS * m_reconnectCount;

    Logger::getInstance().log(LogLevel::WARNING, "WebRTCApp: reconnecting in %d ms (attempt %d)", delay, m_reconnectCount);
    g_timeout_add(delay, reconnectCb, this);
}

gboolean WebRTCApp::reconnectCb(gpointer ud)
{
    static_cast<WebRTCApp*>(ud)->connectToServer();
    return FALSE;
}

void WebRTCApp::onServerConnected_s(SoupSession* s, GAsyncResult* r, gpointer ud)
{
    static_cast<WebRTCApp*>(ud)->onServerConnected(s, r);
}

void WebRTCApp::onWebsocketClosed_s(SoupWebsocketConnection*, gpointer ud)
{
    static_cast<WebRTCApp*>(ud)->onWebsocketClosed();
}

void WebRTCApp::onWebsocketMessage_s(SoupWebsocketConnection* c, SoupWebsocketDataType d,
                                      GBytes* m, gpointer ud)
{ 
    static_cast<WebRTCApp*>(ud)->onWebsocketMessage(c, d, m); 
}

void WebRTCApp::quit()
{
    if(m_loop)
        g_main_loop_quit(m_loop);
}
