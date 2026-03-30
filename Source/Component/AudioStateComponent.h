#pragma once

#include <cstdint>

struct AudioStateComponent
{
    bool isPlaying = false;
    bool isPaused = false;
    bool isVirtualized = false;
    float playbackTimeSec = 0.0f;
    float lengthSec = 0.0f;
    uint64_t activeVoiceHandle = 0;
};
