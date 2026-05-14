#pragma once
// HitStopComponent は timer/speedScale を保持し、関連システムが実行時状態として参照する。

struct HitStopComponent {
    float timer = 0.0f;
    float speedScale = 0.0f;
};
