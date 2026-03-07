#ifndef CAMERA_CONFIG_H
#define CAMERA_CONFIG_H

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
    bool load(const std::string& filePath);

    const std::vector<CameraEntry>& Cameras() const;

    bool is_empty() const;

    size_t size() const;

};

#endif
