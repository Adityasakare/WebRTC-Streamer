#include <gst/gst.h>
#include "../include/Logger.h"

int main(int argc, char* argv[])
{
    gst_init(&argc, &argv);
    Logger& logger = Logger::getInstance();

    logger.log(LogLevel::INFO,"Gstremer Version %s", gst_version_string());

    gst_deinit();

    return 0;
}

