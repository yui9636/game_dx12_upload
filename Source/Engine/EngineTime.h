#pragma once
#include <cstdint>

// エンジン全体で共有する時間情報
struct EngineTime
{
    // 前フレームからの経過時間（秒）
    // ※ Kernelによってタイムスケールやポーズの影響を受けた後の値が入る
    float dt = 0.0f;

    // 影響を受けていない純粋な経過時間（秒）
    // ※ デバッグUIの更新などで使用
    float unscaledDt = 0.0f;

    // アプリ起動からの総経過時間（秒）
    double totalTime = 0.0;

    // タイムスケール（1.0 = 通常, 0.5 = スロー, 0.0 = 停止）
    float timeScale = 1.0f;

    // フレーム数カウント
    uint64_t frameCount = 0;
};