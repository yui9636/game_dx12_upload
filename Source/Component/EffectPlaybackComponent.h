#pragma once

#include <cstdint>

struct EffectPlaybackComponent
{
    bool isPlaying = false;
    float currentTime = 0.0f;
    float duration = 2.0f;
    uint32_t seed = 1;
    bool loop = true;
    bool stopRequested = false;
    uint32_t runtimeInstanceId = 0;
    float lifetimeFade = 1.0f;
};
