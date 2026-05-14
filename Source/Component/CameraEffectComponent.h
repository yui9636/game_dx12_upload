#pragma once
#include <DirectXMath.h>

/**
 * @brief カメラシェイク用の状態。
 */
struct CameraShakeComponent {
    float amplitude = 0.0f;
    float duration = 0.0f;
    float frequency = 0.0f;
    float timer = 0.0f;
    float decay = 1.0f;

    DirectX::XMFLOAT3 currentOffset = { 0, 0, 0 };
};
