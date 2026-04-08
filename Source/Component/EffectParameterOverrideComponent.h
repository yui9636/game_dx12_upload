#pragma once

#include <string>
#include <DirectXMath.h>

struct EffectParameterOverrideComponent
{
    bool enabled = false;
    std::string scalarParameter;
    float scalarValue = 0.0f;
    std::string colorParameter;
    DirectX::XMFLOAT4 colorValue = { 1.0f, 1.0f, 1.0f, 1.0f };
};
