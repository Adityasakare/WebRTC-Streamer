#include <gst/gst.h>
#include <glib.h>
#include <csignal>

#include "../include/Logger.h"
#include "../include/Config.h"
#include "../include/WebRTCApp.h"

static WebRTCApp* g_app = nullptr;

static const gchar* opt_server = DEFAULT_SERVER_URL;
static const gchar* opt_user = "streamer";
static const gchar* opt_config = DEFAULT_CONFIG_PATH;

static GOptionEntry cli_entries[] = {
    { "server", 's', 0, G_OPTION_ARG_STRING, &opt_server, "Server URL", "URL"  },
    { "user",   'u', 0, G_OPTION_ARG_STRING, &opt_user,   "Username",   "NAME" },
    { "config", 'c', 0, G_OPTION_ARG_STRING, &opt_config, "Config file","FILE" },
    { nullptr }
};

static void onSignal(int)
{
    Logger::getInstance().log(LogLevel::INFO, "Shutting down...");
    if (g_app) 
    {
        g_app->quit();
        delete g_app;
        g_app = nullptr;
        exit(0);
    }
}



int main(int argc, char* argv[])
{
    GOptionContext* ctx = g_option_context_new(" WebRTC Streamer");
    g_option_context_add_main_entries(ctx, cli_entries, nullptr);
    g_option_context_add_group(ctx, gst_init_get_option_group());

    GError* err = nullptr;
    if (!g_option_context_parse(ctx, &argc, &argv, &err)) 
    {
        Logger::getInstance().log(LogLevel::ERROR, "%s", err->message);
        return 1;
    }
    
    g_option_context_free(ctx);

    std::signal(SIGINT,  onSignal);
    std::signal(SIGTERM, onSignal);

    Logger::getInstance().log(LogLevel::INFO, "== WebRTC Streamer ==");
    Logger::getInstance().log(LogLevel::INFO, "Server: %s", opt_server);
    Logger::getInstance().log(LogLevel::INFO, "User:    %s", opt_user);
    Logger::getInstance().log(LogLevel::INFO, "Config: %s", opt_config);

    g_app = new WebRTCApp(opt_config, opt_server, opt_user);
    g_app->run();

    delete g_app;
    return 0;
}

