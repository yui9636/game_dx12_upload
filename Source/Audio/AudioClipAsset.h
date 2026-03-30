#pragma once

#include <cstdint>
#include <string>

struct AudioClipAsset
{
    std::string sourcePath;
    std::string importedPath;
    bool streaming = false;
    float defaultVolume = 1.0f;
    float defaultPitch = 1.0f;
    bool defaultLoop = false;
    uint32_t channelCount = 0;
    uint32_t sampleRate = 0;
    float lengthSec = 0.0f;
    uint64_t fileSizeBytes = 0;
    bool valid = false;
};
