// Camera2DComponent は 2D カメラ用の ECS 状態を保持する。
#pragma once

#include <cstdint>

#include <DirectXMath.h>

struct Camera2DComponent
{
    enum class AspectPolicy : uint8_t { Disabled, Fit, Fill };
    enum class ClearMode    : uint8_t { SolidColor, DepthOnly, DontClear };

    float orthographicSize = 10.0f;
    float zoom = 1.0f;
    float nearZ = 0.1f;
    float farZ = 1000.0f;
    DirectX::XMFLOAT4 backgroundColor = { 0.15f, 0.15f, 0.15f, 1.0f };

    DirectX::XMUINT2 referenceResolution = { 1920, 1080 };
    AspectPolicy aspectPolicy = AspectPolicy::Disabled;
    DirectX::XMFLOAT4 letterboxColor = { 0.0f, 0.0f, 0.0f, 1.0f };
    bool pixelSnap = false;
    ClearMode clearMode = ClearMode::SolidColor;
    int priority = 0;
};
