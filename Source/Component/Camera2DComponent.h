#pragma once

#include <DirectXMath.h>

struct Camera2DComponent
{
    float orthographicSize = 10.0f;
    float zoom = 1.0f;
    float nearZ = 0.1f;
    float farZ = 1000.0f;
    DirectX::XMFLOAT4 backgroundColor = { 0.15f, 0.15f, 0.15f, 1.0f };
};
