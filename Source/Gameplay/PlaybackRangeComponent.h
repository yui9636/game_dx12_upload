#pragma once
// PlaybackRangeComponent は enabled/startSeconds/endSeconds/loopWithinRange を保持し、関連システムが実行時状態として参照する。

struct PlaybackRangeComponent {
    bool enabled = false;
    float startSeconds = 0.0f;
    float endSeconds = 0.0f;
    bool loopWithinRange = false;
};
