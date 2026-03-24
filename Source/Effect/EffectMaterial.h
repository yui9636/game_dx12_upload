#pragma once

#include <string>
#include <memory>
#include <array>
#include <DirectXMath.h>
#include <d3d11.h>
#include <wrl/client.h>
#include <nlohmann/json.hpp>

struct RenderContext;

// --------------------------------------------------------
// --------------------------------------------------------
enum class EffectBlendMode
{
    Opaque,
    Alpha,
    Additive,
    Subtractive
};


struct EffectMaterialConstants
{
    // [Block 0] Base Color
    DirectX::XMFLOAT4 baseColor = { 1.0f, 1.0f, 1.0f, 1.0f };

    // [Block 1] Time & Main Scroll
    float emissiveIntensity = 1.0f;
    float currentTime = 0.0f;
    DirectX::XMFLOAT2 mainUvScrollSpeed = { 0.0f, 0.0f };

    // [Block 2] Distortion Params
    float distortionStrength = 0.0f;
    float maskEdgeFade = 0.1f;
    DirectX::XMFLOAT2 distortionUvScrollSpeed = { 0.0f, 0.0f };

    // [Block 3] Dissolve Params
    float dissolveThreshold = 0.0f;
    float dissolveEdgeWidth = 0.05f;

    float maskIntensity = 1.0f;
    float maskContrast = 1.0f;

    // [Block 4] Dissolve Color
    DirectX::XMFLOAT3 dissolveEdgeColor = { 1.0f, 0.5f, 0.2f };
    float _padding3 = 0.0f;

    int mainTexIndex = 0;
    int distortionTexIndex = -1;
    int dissolveTexIndex = -1;
    int maskTexIndex = -1;

    DirectX::XMFLOAT3 fresnelColor = { 0.0f, 0.5f, 1.0f };
    float fresnelPower = 0.0f;

    float flipbookWidth = 1.0f;
    float flipbookHeight = 1.0f;
    float flipbookSpeed = 0.0f;
 
    float gradientStrength = 1.0f;

    int gradientTexIndex = -1;
    float wpoStrength = 0.0f;
    float wpoSpeed = 1.0f;
    float wpoFrequency = 1.0f;

    float chromaticAberrationStrength = 0.0f;
   
    float dissolveGlowIntensity = 0.0f;
    float dissolveGlowRange = 0.1f;
  
    int uvScrollMode = 0;

    DirectX::XMFLOAT3 dissolveGlowColor = { 1.0f, 0.5f, 0.0f };

    int matCapTexIndex = -1;

    float matCapStrength = 1.0f;
    float matCapBlend = 0.5f;
    float clipSoftness=0.01f;
    float startEndFadeWidth=0.0f;


    DirectX::XMFLOAT3 matCapColor = { 1.0f, 1.0f, 1.0f };
 
    int normalTexIndex = -1;    

    float normalStrength = 1.0f;
    float _padding13_1 = 0.0f;
    float _padding13_2 = 0.0f;
    float _padding13_3 = 0.0f;

    // [Block 14] Flow Map Parameters
    int flowTexIndex = -1;
    float flowStrength = 0.1f;
    float flowSpeed = 1.0f;
    float sideFadeWidth = 0.0f;

    // [Block 15]
    float visibility = 1.0f;
    float clipStart = 0.0f;
    float clipEnd = 1.0f;
    float _padding15 = 0.0f;

    // --------------------------------------------------------
    // --------------------------------------------------------
    int subTexIndex = -1;
    int subBlendMode = 0;
    DirectX::XMFLOAT2 subUvScrollSpeed = { 0.0f, 0.0f };

    float subTexStrength = 1.0f;
    float subUvRotationSpeed = 0.0f;
    float usePolarCoords = 0.0f;
   
    float toonThreshold = 0.5f;

    float toonSmoothing = 0.01f;
    float toonSteps = 0.0f;

    float toonNoiseStrength = 0.0f;
    float toonNoiseSpeed = 0.5f;

    DirectX::XMFLOAT4 toonShadowColor = { 0.0f, 0.0f, 0.0f, 0.0f };

    int toonNoiseTexIndex = -1;
    float _padding20_1 = 0.0f;
    float _padding20_2 = 0.0f;
    float _padding20_3 = 0.0f;
    DirectX::XMFLOAT4 toonSpecular = { 0.0f, 0.0f, 0.0f, 0.0f };

    int toonRampTexIndex = -1;
    float _padding22_1 = 0.0f;
    float _padding22_2 = 0.0f;
    float _padding22_3 = 0.0f;

};



// --------------------------------------------------------
// --------------------------------------------------------
class EffectMaterial
{
public:
    EffectMaterial();
    ~EffectMaterial() = default;

    static const int TEXTURE_SLOT_COUNT = 10;

    void SetTexture(int slot, const std::string& path, Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> texture);

    std::string GetTexturePath(int slot) const;

    void SetColor(const DirectX::XMFLOAT4& color) { constants.baseColor = color; }
    void SetBlendMode(EffectBlendMode mode) { blendMode = mode; }

    EffectMaterialConstants& GetConstants() { return constants; }
    EffectBlendMode GetBlendMode() const { return blendMode; }

    void Apply(const RenderContext& rc);

private:
    void CreateConstantBuffer();

    EffectMaterialConstants constants;
    EffectBlendMode blendMode = EffectBlendMode::Additive;

    Microsoft::WRL::ComPtr<ID3D11Buffer> constantBuffer;

    std::array<Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>, TEXTURE_SLOT_COUNT> textures;

    std::array<std::string, TEXTURE_SLOT_COUNT> texturePaths;
};


