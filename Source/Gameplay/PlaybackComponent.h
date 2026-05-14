#pragma once
// PlaybackComponent は currentSeconds/clipLength/playSpeed/playing を保持し、関連システムが実行時状態として参照する。

struct PlaybackComponent {
    float currentSeconds = 0.0f;
    float clipLength = 0.0f;
    float playSpeed = 1.0f;
    bool playing = false;
    bool looping = true;
    bool stopAtEnd = false;
    bool finished = false;
};
