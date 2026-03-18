#pragma once
#include <string>
#include <DirectXMath.h>
#include "JSONManager.h" 

class MaterialAsset {
public:
    MaterialAsset(const std::string& filePath);
    ~MaterialAsset() = default;

    void Load();
    void Save();

    const std::string& GetFilePath() const { return m_filePath; }

    // ==========================================
    // マテリアルパラメータ（PBR対応）
    // ==========================================
    DirectX::XMFLOAT4 baseColor = { 1.0f, 1.0f, 1.0f, 1.0f };
    float metallic = 0.0f;
    float roughness = 1.0f;
    float emissive = 0.0f;

    std::string diffuseTexturePath;
    std::string normalTexturePath;
    std::string metallicRoughnessTexturePath;
    std::string emissiveTexturePath;

    int shaderId = 1;  // 1: PBR
    int alphaMode = 0; // 0: Opaque, 1: Mask, 2: Blend

private:
    std::string m_filePath;
};