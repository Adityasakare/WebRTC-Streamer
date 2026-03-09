#ifndef CAMERA_CONFIG_H
#define CAMERA_CONFIG_H

// Parses a camera config file into CameraEntry structs.

#include <string>
#include <vector>
#include <fstream>
#include <sstream>
 #include <unistd.h>
#include "Logger.h"

#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/videodev2.h>

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
            
            if (!isCameraWorking(devicePath))
            {
                Logger::getInstance().log(LogLevel::WARNING, "CameraConfig: skipping '%s' — not found, not accessible, or not a video capture device",
                devicePath.c_str());
                continue;
            }
                
            
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

    bool isCameraWorking(const std::string& devicePath)
    {
        //check file exists and is readable
        if (access(devicePath.c_str(), F_OK | R_OK) != 0)
            return false;

        // open the device
        int fd = open(devicePath.c_str(), O_RDONLY | O_NONBLOCK);
        if (fd < 0)
            return false;

        // query device capabilities via ioctl
        struct v4l2_capability cap;
        int ret = ioctl(fd, VIDIOC_QUERYCAP, &cap);
        close(fd);

        if (ret < 0)
            return false;

        // confirm it supports video capture
        if (!(cap.device_caps & V4L2_CAP_VIDEO_CAPTURE))
            return false;

        return true;
    }
};

#endif
