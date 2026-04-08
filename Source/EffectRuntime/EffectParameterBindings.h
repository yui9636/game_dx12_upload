#pragma once

#include <algorithm>
#include <cctype>
#include <cmath>
#include <string>
#include <string_view>

#include "EffectGraphAsset.h"

namespace EffectParameterBindings
{
    inline std::string NormalizeName(std::string_view name)
    {
        std::string result;
        result.reserve(name.size());
        for (char c : name) {
            if (c == ' ' || c == '_' || c == '-' || c == '\t') {
                continue;
            }
            result.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
        }
        return result;
    }

    inline DirectX::XMFLOAT4 MultiplyColor(const DirectX::XMFLOAT4& a, const DirectX::XMFLOAT4& b)
    {
        return { a.x * b.x, a.y * b.y, a.z * b.z, a.w * b.w };
    }

    inline const char* DescribeBinding(EffectValueType valueType, std::string_view name)
    {
        const std::string normalized = NormalizeName(name);
        if (valueType == EffectValueType::Color) {
            if (normalized == "tint" || normalized == "color") return "Tint all renderers";
            if (normalized == "startcolor") return "Particle start color";
            if (normalized == "endcolor" || normalized == "tintend") return "Particle end color";
            if (normalized == "meshcolor" || normalized == "meshtint") return "Mesh renderer tint";
            return nullptr;
        }

        if (valueType == EffectValueType::Float) {
            if (normalized == "intensity" || normalized == "brightness") return "Multiply RGB intensity";
            if (normalized == "opacity" || normalized == "alpha") return "Multiply alpha";
            if (normalized == "spawnrate" || normalized == "rate") return "Particle spawn rate";
            if (normalized == "burst" || normalized == "burstcount") return "Particle burst count";
            if (normalized == "lifetime" || normalized == "duration") return "Lifetime and duration";
            if (normalized == "speed") return "Particle speed";
            if (normalized == "drag") return "Particle drag";
            if (normalized == "size" || normalized == "sizescale") return "Scale particle size";
            if (normalized == "startsize") return "Particle start size";
            if (normalized == "endsize") return "Particle end size";
            if (normalized == "ribbonwidth") return "Ribbon width";
            if (normalized == "ribbonstretch" || normalized == "ribbonvelocitystretch") return "Ribbon velocity stretch";
            if (normalized == "alphascale") return "Scale alpha";
            if (normalized == "sizecurvebias") return "Size curve bias";
            if (normalized == "alphacurvebias") return "Alpha curve bias";
        }

        return nullptr;
    }

    inline bool ApplyFloatParameter(
        std::string_view name,
        float value,
        EffectMeshRendererDescriptor& meshRenderer,
        EffectParticleSimulationLayout& particleRenderer,
        float& duration)
    {
        const std::string normalized = NormalizeName(name);
        if (normalized.empty()) {
            return false;
        }

        if (normalized == "intensity" || normalized == "brightness") {
            meshRenderer.tint.x *= value;
            meshRenderer.tint.y *= value;
            meshRenderer.tint.z *= value;
            particleRenderer.tint.x *= value;
            particleRenderer.tint.y *= value;
            particleRenderer.tint.z *= value;
            particleRenderer.tintEnd.x *= value;
            particleRenderer.tintEnd.y *= value;
            particleRenderer.tintEnd.z *= value;
            return true;
        }
        if (normalized == "opacity" || normalized == "alpha") {
            meshRenderer.tint.w *= value;
            particleRenderer.tint.w *= value;
            particleRenderer.tintEnd.w *= value;
            return true;
        }
        if (normalized == "spawnrate" || normalized == "rate") {
            particleRenderer.spawnRate = (std::max)(0.0f, value);
            return true;
        }
        if (normalized == "burst" || normalized == "burstcount") {
            particleRenderer.burstCount = static_cast<uint32_t>((std::max)(0.0f, std::round(value)));
            return true;
        }
        if (normalized == "lifetime" || normalized == "duration") {
            const float safeValue = (std::max)(0.01f, value);
            particleRenderer.particleLifetime = safeValue;
            duration = safeValue;
            return true;
        }
        if (normalized == "speed") {
            particleRenderer.speed = (std::max)(0.0f, value);
            return true;
        }
        if (normalized == "drag") {
            particleRenderer.drag = (std::max)(0.0f, value);
            return true;
        }
        if (normalized == "size" || normalized == "sizescale") {
            const float scale = (std::max)(0.0f, value);
            particleRenderer.startSize *= scale;
            particleRenderer.endSize *= scale;
            return true;
        }
        if (normalized == "startsize") {
            particleRenderer.startSize = (std::max)(0.0f, value);
            return true;
        }
        if (normalized == "endsize") {
            particleRenderer.endSize = (std::max)(0.0f, value);
            return true;
        }
        if (normalized == "ribbonwidth") {
            particleRenderer.ribbonWidth = (std::max)(0.01f, value);
            return true;
        }
        if (normalized == "ribbonstretch" || normalized == "ribbonvelocitystretch") {
            particleRenderer.ribbonVelocityStretch = (std::max)(0.0f, value);
            return true;
        }
        if (normalized == "alphascale") {
            meshRenderer.tint.w *= value;
            particleRenderer.tint.w *= value;
            particleRenderer.tintEnd.w *= value;
            return true;
        }
        if (normalized == "sizecurvebias") {
            particleRenderer.sizeCurveBias = (std::max)(0.05f, value);
            return true;
        }
        if (normalized == "alphacurvebias") {
            particleRenderer.alphaCurveBias = (std::max)(0.05f, value);
            return true;
        }

        return false;
    }

    inline bool ApplyColorParameter(
        std::string_view name,
        const DirectX::XMFLOAT4& value,
        EffectMeshRendererDescriptor& meshRenderer,
        EffectParticleSimulationLayout& particleRenderer)
    {
        const std::string normalized = NormalizeName(name);
        if (normalized.empty()) {
            return false;
        }

        if (normalized == "tint" || normalized == "color") {
            meshRenderer.tint = MultiplyColor(meshRenderer.tint, value);
            particleRenderer.tint = MultiplyColor(particleRenderer.tint, value);
            particleRenderer.tintEnd = MultiplyColor(particleRenderer.tintEnd, value);
            return true;
        }
        if (normalized == "startcolor") {
            meshRenderer.tint = MultiplyColor(meshRenderer.tint, value);
            particleRenderer.tint = MultiplyColor(particleRenderer.tint, value);
            return true;
        }
        if (normalized == "endcolor" || normalized == "tintend") {
            particleRenderer.tintEnd = MultiplyColor(particleRenderer.tintEnd, value);
            return true;
        }
        if (normalized == "meshcolor" || normalized == "meshtint") {
            meshRenderer.tint = MultiplyColor(meshRenderer.tint, value);
            return true;
        }

        return false;
    }
}
