#pragma once
#include <DirectXMath.h>

/**
 * @brief ・ｽJ・ｽ・ｽ・ｽ・ｽ・ｽV・ｽF・ｽC・ｽN・ｽi・ｽU・ｽ・ｽ・ｽj
 */
struct CameraShakeComponent {
    float amplitude = 0.0f;
    float duration = 0.0f;
    float frequency = 0.0f;
    float timer = 0.0f;
    float decay = 1.0f;

    DirectX::XMFLOAT3 currentOffset = { 0, 0, 0 };
};
