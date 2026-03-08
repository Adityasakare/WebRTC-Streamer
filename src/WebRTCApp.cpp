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

