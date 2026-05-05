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

    json.Save();
}
