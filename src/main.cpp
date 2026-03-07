#include "../include/Logger.h"

int main(void)
{
    Logger& logger = Logger::getInstance();

    logger.log(LogLevel::INFO,"Hello from %s", __FILE__);
    logger.log(LogLevel::WARNING, "Test warning %d", 42);
    logger.log(LogLevel::ERROR, "Test error %s", "oops");
    logger.log(LogLevel::WARNING, "The END");

    return 0;

    return 0;
}

