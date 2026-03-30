#pragma once

#include <string>

enum class AudioBusType
{
    Master = 0,
    BGM = 1,
    SFX = 2,
    UI = 3
};

struct AudioEmitterComponent
{
    std::string clipAssetPath;
    bool playOnStart = false;
    bool loop = false;
    bool is3D = true;
    float volume = 1.0f;
    float pitch = 1.0f;
    float spatialBlend = 1.0f;
    float minDistance = 1.0f;
    float maxDistance = 50.0f;
    bool streaming = false;
    AudioBusType bus = AudioBusType::SFX;
};
