#pragma once

#include <string>
#include <vector>
#include <DirectXMath.h>

struct EffectParameterOverrideComponent
{
    bool enabled = false;
    // 旧形式との互換用に残す単一パラメータ項目。
    std::string scalarParameter;
    float scalarValue = 0.0f;
    std::string colorParameter;
    DirectX::XMFLOAT4 colorValue = { 1.0f, 1.0f, 1.0f, 1.0f };

    // Phase 1B の複数パラメータ override。
    std::vector<std::string> scalarNames;
    std::vector<float> scalarValues;
    std::vector<std::string> colorNames;
    std::vector<DirectX::XMFLOAT4> colorValues;
};
