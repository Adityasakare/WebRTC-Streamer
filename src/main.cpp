#include <gst/gst.h>
#include <csignal>
#include "../include/Logger.h"

static GMainLoop* g_loop = nullptr;

static void buildRecordPipeline(const std::string& device)
{
    GError* err = nullptr;

    std::string desc =
        "v4l2src device=" + device + " do-timestamp=true ! "
        "videoconvert ! videoscale ! "
        "video/x-raw,width=640,height=480,framerate=30/1 ! "
        "x264enc bitrate=1000 speed-preset=ultrafast tune=zerolatency key-int-max=30 ! "
        "video/x-h264,profile=baseline ! "
        "h264parse config-interval=-1 ! "
        "mp4mux ! "
        "filesink location=/tmp/test_record.mp4";

    GstElement* pipeline = gst_parse_launch(desc.c_str(), &err);

    if (err) 
    {
        Logger::getInstance().log(LogLevel::ERROR, "Pipeline error: %s", err->message);
        g_error_free(err);
        return;
    }

    gst_element_set_state(pipeline, GST_STATE_PLAYING);
    Logger::getInstance().log(LogLevel::INFO, "Recording for 5 seconds...");

    g_main_loop_run(g_loop);

    gst_element_set_state(pipeline, GST_STATE_NULL);
    gst_object_unref(pipeline);

    Logger::getInstance().log(LogLevel::INFO, "Done. Check /tmp/test_record.mp4");
}



static void onSignal(int)
{
    Logger::getInstance().log(LogLevel::INFO, "Interrupt — stopping...");
    if (g_loop) 
        g_main_loop_quit(g_loop);
}


int main(int argc, char* argv[])
{
    gst_init(&argc, &argv);
    
    std::signal(SIGINT,  onSignal);
    std::signal(SIGTERM, onSignal);

    g_loop = g_main_loop_new(nullptr, FALSE);

    buildRecordPipeline("/dev/video0");

    gst_deinit();

    return 0;
}

