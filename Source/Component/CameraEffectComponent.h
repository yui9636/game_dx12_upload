#pragma once
#include <DirectXMath.h>

/**
 * @brief カメラシェイク（振動）
 */
struct CameraShakeComponent {
    float amplitude = 0.0f;
    float duration = 0.0f;
    float frequency = 0.0f;
    float timer = 0.0f;
    float decay = 1.0f;

    // システムが計算したオフセット量
    DirectX::XMFLOAT3 currentOffset = { 0, 0, 0 };
};