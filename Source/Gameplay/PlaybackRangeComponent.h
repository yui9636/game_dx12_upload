#pragma once

struct PlaybackRangeComponent {
    bool enabled = false;
    float startSeconds = 0.0f;
    float endSeconds = 0.0f;
    bool loopWithinRange = false;
};
