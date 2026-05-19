#include "MaterialAsset.h"
#include <filesystem>

// ファイルパスを受け取り、既存ファイルがあれば即座に読み込みます。
MaterialAsset::MaterialAsset(const std::string& filePath) : m_filePath(filePath) {
    Load();
}

// JSON ファイルからマテリアルの各パラメータを読み込みます。
// ファイルが存在しない場合は、既定値のまま処理を終了します。
void MaterialAsset::Load() {
    if (!std::filesystem::exists(m_filePath)) return;

    JSONManager json(m_filePath);

    baseColor = json.Get<DirectX::XMFLOAT4>("baseColor", { 1.0f, 1.0f, 1.0f, 1.0f });
    metallic = json.Get<float>("metallic", 0.0f);
    roughness = json.Get<float>("roughness", 1.0f);
    emissive = json.Get<float>("emissive", 0.0f);

    diffuseTexturePath = json.Get<std::string>("diffuseTexturePath", "");
    normalTexturePath = json.Get<std::string>("normalTexturePath", "");
    metallicRoughnessTexturePath = json.Get<std::string>("metallicRoughnessTexturePath", "");
    emissiveTexturePath = json.Get<std::string>("emissiveTexturePath", "");

    shaderId = json.Get<int>("shaderId", 1);
    alphaMode = json.Get<int>("alphaMode", 0);

    // Toon (basic)
    toonShadingMode = json.Get<int>("toonShadingMode", 0);
    toonShadowTint  = json.Get<DirectX::XMFLOAT3>("toonShadowTint", { 0.55f, 0.50f, 0.65f });
    toonShadowDeep  = json.Get<DirectX::XMFLOAT3>("toonShadowDeep", { 0.25f, 0.22f, 0.38f });
    toonShadowMidThreshold  = json.Get<float>("toonShadowMidThreshold", 0.55f);
    toonShadowDeepThreshold = json.Get<float>("toonShadowDeepThreshold", 0.28f);
    toonRimColor    = json.Get<DirectX::XMFLOAT3>("toonRimColor",   { 1.0f, 1.0f, 1.0f });
    toonRimPower    = json.Get<float>("toonRimPower", 4.0f);
    toonRimStrength = json.Get<float>("toonRimStrength", 0.4f);
    toonBandLevels  = json.Get<int>("toonBandLevels", 3);
    toonUseRampTexture  = json.Get<int>("toonUseRampTexture", 0) != 0;
    toonRampTexturePath = json.Get<std::string>("toonRampTexturePath", "");
    // 旧 useRampTexture フラグから shadingMode へ移行
    if (toonUseRampTexture && toonShadingMode == 0) toonShadingMode = 2;

    // Toon (outline)
    toonOutlineEnabled = json.Get<int>("toonOutlineEnabled", 1) != 0;
    toonOutlineColor   = json.Get<DirectX::XMFLOAT3>("toonOutlineColor", { 0.05f, 0.05f, 0.08f });
    toonOutlineWidth   = json.Get<float>("toonOutlineWidth", 0.012f);
    toonOutlineDistanceScale = json.Get<float>("toonOutlineDistanceScale", 0.0f);

    // Toon (banded specular)
    toonSpecColor     = json.Get<DirectX::XMFLOAT3>("toonSpecColor", { 1.0f, 1.0f, 1.0f });
    toonSpecStrength  = json.Get<float>("toonSpecStrength", 0.0f);
    toonSpecSharpness = json.Get<float>("toonSpecSharpness", 0.5f);
    toonSpecThreshold = json.Get<float>("toonSpecThreshold", 0.5f);

    // Toon (fixed light)
    toonUseFixedLight = json.Get<int>("toonUseFixedLight", 0) != 0;
    toonFixedLightDir = json.Get<DirectX::XMFLOAT3>("toonFixedLightDir", { 0.0f, -0.5f, -0.7f });

    // Toon (anisotropic)
    toonUseAnisotropic = json.Get<int>("toonUseAnisotropic", 0) != 0;
    toonAnisoSharpness = json.Get<float>("toonAnisoSharpness", 0.5f);
    toonAnisoOffset    = json.Get<float>("toonAnisoOffset", 0.0f);
}

// 現在保持しているマテリアルの各パラメータを JSON ファイルへ保存します。
void MaterialAsset::Save() {
    JSONManager json(m_filePath);

    json.Set("baseColor", baseColor);
    json.Set("metallic", metallic);
    json.Set("roughness", roughness);
    json.Set("emissive", emissive);

    json.Set("diffuseTexturePath", diffuseTexturePath);
    json.Set("normalTexturePath", normalTexturePath);
    json.Set("metallicRoughnessTexturePath", metallicRoughnessTexturePath);
    json.Set("emissiveTexturePath", emissiveTexturePath);

    json.Set("shaderId", shaderId);
    json.Set("alphaMode", alphaMode);

    // Toon
    json.Set("toonShadingMode", toonShadingMode);
    json.Set("toonShadowTint", toonShadowTint);
    json.Set("toonShadowDeep", toonShadowDeep);
    json.Set("toonShadowMidThreshold", toonShadowMidThreshold);
    json.Set("toonShadowDeepThreshold", toonShadowDeepThreshold);
    json.Set("toonRimColor", toonRimColor);
    json.Set("toonRimPower", toonRimPower);
    json.Set("toonRimStrength", toonRimStrength);
    json.Set("toonBandLevels", toonBandLevels);
    json.Set("toonUseRampTexture", toonUseRampTexture ? 1 : 0);
    json.Set("toonRampTexturePath", toonRampTexturePath);
    json.Set("toonOutlineEnabled", toonOutlineEnabled ? 1 : 0);
    json.Set("toonOutlineColor", toonOutlineColor);
    json.Set("toonOutlineWidth", toonOutlineWidth);
    json.Set("toonOutlineDistanceScale", toonOutlineDistanceScale);
    json.Set("toonSpecColor", toonSpecColor);
    json.Set("toonSpecStrength", toonSpecStrength);
    json.Set("toonSpecSharpness", toonSpecSharpness);
    json.Set("toonSpecThreshold", toonSpecThreshold);
    json.Set("toonUseFixedLight", toonUseFixedLight ? 1 : 0);
    json.Set("toonFixedLightDir", toonFixedLightDir);
    json.Set("toonUseAnisotropic", toonUseAnisotropic ? 1 : 0);
    json.Set("toonAnisoSharpness", toonAnisoSharpness);
    json.Set("toonAnisoOffset", toonAnisoOffset);

    json.Save();
}
