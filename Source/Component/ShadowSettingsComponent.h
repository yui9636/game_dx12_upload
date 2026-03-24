#pragma once
#include <DirectXMath.h>

struct ShadowSettingsComponent {
    bool enableShadow = true;
    DirectX::XMFLOAT3 shadowColor = { 0.1f, 0.1f, 0.1f };
};
