#ifndef CAMERA_CONFIG_H
#define CAMERA_CONFIG_H

// Parses a camera config file into CameraEntry structs.

#include <string>
#include <vector>
#include <fstream>
#include <sstream>
#include "Logger.h"

struct CameraEntry
{
    std::string devicePath;
    std::string displayName; 
};

class CameraConfig
{
private:
    std::vector <CameraEntry> m_cameras;
    Logger& logger = Logger::getInstance();
public:
    bool load(const std::string& filePath)
    {
        m_cameras.clear();
        // Open the file
        std::ifstream file(filePath.c_str());
        if(!file.is_open())
        {
            logger.log(LogLevel::ERROR, "CameraConfig: cannot open '%s'", filePath.c_str());
            return false;
        }

        std::string line;
        while(std::getline(file, line))
        {
            // skip blank lines and comments
            size_t start = line.find_first_not_of(" \t");
            if(start == std::string::npos || line[start] == '#')
                continue;
            // Remove the leading whitespace
            line = line.substr(start);
            // split the line into tokens using a string stream
            std::istringstream iss(line);
            std::string devicePath;
            iss >> devicePath;
            if(devicePath.empty())
                continue;
            // grab the remainder of the line as the display name
            std::string displayName;
            std::getline(iss, displayName);
            // trim leading whitespace from the display name
            size_t nameStart = displayName.find_first_not_of(" \t");
            displayName = (nameStart != std::string::npos)
                        ? displayName.substr(nameStart)
                        : devicePath;
            // store the parsed cameras
            m_cameras.push_back({devicePath, displayName});
            logger.log(LogLevel::INFO, "CameraConfig: [%s] -> '%s'", filePath.c_str(), displayName.c_str());

        }

        if(m_cameras.empty())
        {
            logger.log(LogLevel::WARNING, "CameraConfig: no cameras found in '%s'", filePath.c_str());
            return false;
        }

        logger.log(LogLevel::INFO, "CameraConfig: %zu cameras configured", m_cameras.size());
        return true;
    }

    const std::vector<CameraEntry>& Cameras() const
    {
        return m_cameras;
    }

    bool is_empty() const
    {
        return m_cameras.empty();
    }

    size_t size() const
    {
        return m_cameras.size();
    }

};

#endif
