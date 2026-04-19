#pragma once

#include <DirectXMath.h>
#include <cstdint>
#include <string>

enum EffectMeshShaderFlags : uint32_t
{
    MeshFlag_None                = 0,
    MeshFlag_Texture             = 1 << 0,
    MeshFlag_Dissolve            = 1 << 1,
    MeshFlag_Distort             = 1 << 2,
    MeshFlag_Lighting            = 1 << 3,
    MeshFlag_Mask                = 1 << 4,
    MeshFlag_Fresnel             = 1 << 5,
    MeshFlag_Flipbook            = 1 << 6,
    MeshFlag_GradientMap         = 1 << 7,
    MeshFlag_ChromaticAberration = 1 << 8,
    MeshFlag_DissolveGlow        = 1 << 9,
    MeshFlag_MatCap              = 1 << 10,
    MeshFlag_NormalMap           = 1 << 11,
    MeshFlag_FlowMap             = 1 << 12,
    MeshFlag_SideFade            = 1 << 13,
    MeshFlag_AlphaFade           = 1 << 14,
    MeshFlag_SubTexture          = 1 << 15,
    MeshFlag_Toon                = 1 << 16,
    MeshFlag_RimLight            = 1 << 17,
    MeshFlag_VertexColorBlend    = 1 << 18,
    MeshFlag_Emission            = 1 << 19,
    MeshFlag_Scroll              = 1 << 20,
};

namespace MeshVariantPreset
{
    constexpr uint32_t Slash_Basic      = 0x00004003u;
    constexpr uint32_t Slash_Glow       = 0x00006003u;
    constexpr uint32_t Slash_Flow       = 0x00014021u;
    constexpr uint32_t Slash_Full       = 0x00036023u;
    constexpr uint32_t Magic_Circle     = 0x00014011u;
    constexpr uint32_t Magic_Summon     = 0x0000080Bu;
    constexpr uint32_t Magic_Aura       = 0x00024021u;
    constexpr uint32_t Magic_Explosion  = 0x00004105u;
    constexpr uint32_t Universal_Glow   = 0x00084021u;
    constexpr uint32_t Universal_Flow   = 0x00018001u;
}

struct EffectMeshEffectConstants
{
    float                    dissolveAmount    = 0.0f;
    float                    dissolveEdge      = 0.05f;
    DirectX::XMFLOAT2        flowSpeed         = { 0.1f, 0.0f };

    DirectX::XMFLOAT4        dissolveGlowColor = { 1.0f, 0.6f, 0.1f, 1.0f };

    float                    fresnelPower      = 3.0f;
    DirectX::XMFLOAT3        _pad0             = {};
    DirectX::XMFLOAT4        fresnelColor      = { 1.0f, 1.0f, 1.0f, 1.0f };

    float                    flowStrength      = 0.3f;
    float                    alphaFade         = 1.0f;
    DirectX::XMFLOAT2        scrollSpeed       = { 0.0f, 0.0f };

    float                    distortStrength   = 0.1f;
    DirectX::XMFLOAT3        _pad1             = {};

    DirectX::XMFLOAT4        rimColor          = { 1.0f, 1.0f, 1.0f, 1.0f };
    float                    rimPower          = 2.0f;
    DirectX::XMFLOAT3        _pad2             = {};

    DirectX::XMFLOAT4        emissionColor     = { 1.0f, 1.0f, 1.0f, 1.0f };
    float                    emissionIntensity = 0.0f;
    float                    effectTime        = 0.0f;
    DirectX::XMFLOAT2        _pad3             = {};
};
static_assert(sizeof(EffectMeshEffectConstants) % 16 == 0,
    "EffectMeshEffectConstants must be 16-byte aligned");

struct EffectMeshVariantParams
{
    uint32_t shaderFlags = MeshFlag_Texture | MeshFlag_AlphaFade;
    EffectMeshEffectConstants constants;

    // Base (albedo) texture authored on the MeshRenderer node (AssetPickerKind::Texture).
    // When non-empty this overrides the FBX material's albedoMap for slot 0 in
    // EffectMeshPass, so effect templates can specify their own base texture
    // (e.g. Aura01_T.png) without editing the source model material.
    std::string baseTexturePath;
    std::string maskTexturePath;
    std::string normalMapPath;
    std::string flowMapPath;
    std::string subTexturePath;
    std::string emissionTexPath;
};
