#include "../include/Logger.h"

Logger::Logger()
{
    logFile.open("log.txt", std::ios::out | std::ios::app);
    if(!logFile.is_open())
    {
        std::cerr << "Failed to open a log file" << std::endl;
    }
}

Logger::~Logger()
{
    if(logFile.is_open())
    {
        logFile.close();
    }
}

std::string Logger::getLevelString(LogLevel level)
{
    switch (level)
    {
    case LogLevel::WARNING:
        return "WARNING";
    case LogLevel::ERROR:
        return "ERROR";
    default:
        return "INFO";
    }
}


Logger& Logger::getInstance()
{
    static Logger instace;
    return instace;
} 


void Logger::log(LogLevel level, const char* msg, ...)
{
    std::lock_guard<std::mutex> lock(logMutex);

    if(logFile.is_open())
    {
        std::time_t currentTime = std::time(nullptr);
        char timestamp[32];
        std::strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", std::localtime(&currentTime));

        char buffer[1024];
        va_list args;
        va_start(args, msg);
        vsnprintf(buffer, sizeof(buffer), msg, args);
        va_end(args);

        logFile << "[" << timestamp << "] [" << getLevelString(level) << "] " << buffer << std::endl;
        std::cout << "[" << timestamp << "] [" << getLevelString(level) << "] " << buffer << std::endl; 
    }
}

int main(void)
{
    Logger& logger = Logger::getInstance();

    logger.log(LogLevel::INFO,"Hello from %s", __FILE__);
    logger.log(LogLevel::WARNING, "Test warning %d", 42);
    logger.log(LogLevel::ERROR, "Test error %s", "oops");
    logger.log(LogLevel::WARNING, "The END");

    return 0;
}