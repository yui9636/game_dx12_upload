#pragma once

#include <DirectXMath.h>
#include <vector>

struct TrailComponent
{
    bool enabled = true;
    float width = 0.15f;
    float lifetime = 0.5f;        // seconds before trail segment fades
    float minDistance = 0.02f;     // minimum distance between samples
    int maxPoints = 64;

    DirectX::XMFLOAT4 colorStart = { 1.0f, 1.0f, 1.0f, 1.0f };
    DirectX::XMFLOAT4 colorEnd   = { 1.0f, 1.0f, 1.0f, 0.0f };

    // Runtime state
    struct TrailPoint
    {
        DirectX::XMFLOAT3 position;
        DirectX::XMFLOAT3 up;      // local up for ribbon orientation
        float timeStamp;           // absolute time when sampled
    };
    std::vector<TrailPoint> points;
    float totalTime = 0.0f;

    void Clear() { points.clear(); totalTime = 0.0f; }
};
