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
        g_object_unref(m_loop);
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

