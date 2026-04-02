#pragma once

struct PlaybackComponent {
    float currentSeconds = 0.0f;
    float clipLength = 0.0f;
    float playSpeed = 1.0f;
    bool playing = false;
    bool looping = true;
    bool stopAtEnd = false;
    bool finished = false;
};
