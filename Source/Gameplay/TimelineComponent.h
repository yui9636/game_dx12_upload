#pragma once
#include <cstdint>
// TimelineComponent は fps/currentFrame/frameMin/frameMax を保持し、関連システムが実行時状態として参照する。

struct TimelineComponent {
    float fps = 60.0f;
    int previousFrame = -1;
    int currentFrame = 0;
    int frameMin = 0;
    int frameMax = 600;
    int animationIndex = -1;
    float clipLengthSec = 10.0f;
    bool playing = false;
};
