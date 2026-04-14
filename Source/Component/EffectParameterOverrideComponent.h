#pragma once

#include <string>
#include <vector>
#include <DirectXMath.h>

struct EffectParameterOverrideComponent
{
    bool enabled = false;
    // Legacy single-parameter fields (backward compat)
    std::string scalarParameter;
    float scalarValue = 0.0f;
    std::string colorParameter;
    DirectX::XMFLOAT4 colorValue = { 1.0f, 1.0f, 1.0f, 1.0f };

    // Multi-parameter overrides (Phase 1B)
    std::vector<std::string> scalarNames;
    std::vector<float> scalarValues;
    std::vector<std::string> colorNames;
    std::vector<DirectX::XMFLOAT4> colorValues;
};
