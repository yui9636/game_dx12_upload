#pragma once
#include <DirectXMath.h>

enum class LightType {
    Directional,
    Point,
    Spot
};

struct LightComponent {
    LightType type = LightType::Point;
    DirectX::XMFLOAT3 color = { 1.0f, 1.0f, 1.0f };
    float intensity = 1.0f;
    float range = 10.0f;

    bool castShadow = false;

};
