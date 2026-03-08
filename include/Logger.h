#ifndef LOGGER_H
#define LOGGER_H

#include <iostream>
#include <fstream>
#include <string>
#include <mutex>
#include <ctime>
#include <cstdarg>


enum LogLevel
{
    INFO = 1,
    WARNING,
    ERROR
};


class Logger
{
private:
    std::ofstream logFile;
    std::mutex logMutex;

    Logger();
    ~Logger();
    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;

    std::string getLevelString(LogLevel level);

public:
    static Logger& getInstance();
    void log(LogLevel level, const char* msg, ...);
};



#endif
