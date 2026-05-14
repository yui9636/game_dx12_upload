#pragma once

#include <DirectXMath.h>
#include <vector>

struct TrailComponent
{
    bool enabled = true;
    float width = 0.15f;
    float lifetime = 0.5f;        // trail segment がフェードするまでの秒数。
    float minDistance = 0.02f;     // sample を追加する最小距離。
    int maxPoints = 64;

    DirectX::XMFLOAT4 colorStart = { 1.0f, 1.0f, 1.0f, 1.0f };
    DirectX::XMFLOAT4 colorEnd   = { 1.0f, 1.0f, 1.0f, 0.0f };

    // 実行時に更新される状態。
    struct TrailPoint
    {
        DirectX::XMFLOAT3 position;
        DirectX::XMFLOAT3 up;      // ribbon の向きを決める local up。
        float timeStamp;           // sample した絶対時刻。
    };
    std::vector<TrailPoint> points;
    float totalTime = 0.0f;

    void Clear() { points.clear(); totalTime = 0.0f; }
};
