#pragma once
#include <DirectXMath.h>

struct GridComponent {
    int subdivisions = 20;
    float scale = 1.0f;
    DirectX::XMFLOAT4 color = { 1.0f, 1.0f, 1.0f, 1.0f };
    bool enabled = true;
};
