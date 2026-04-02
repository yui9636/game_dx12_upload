#pragma once
#include <cstdint>

struct TimelineComponent {
    float fps = 60.0f;
    int currentFrame = 0;
    int frameMin = 0;
    int frameMax = 600;
    int animationIndex = -1;
    float clipLengthSec = 10.0f;
    bool playing = false;
};
