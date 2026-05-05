#pragma once
#include <string>
#include <DirectXMath.h>
#include "JSONManager.h"

// マテリアル設定ファイルを読み書きするためのアセットクラス。
// 色・金属度・粗さ・各種テクスチャパスなど、エディタで編集する値を保持します。
class MaterialAsset {
public:
    // 指定されたファイルパスを保持し、存在する場合はマテリアル情報を読み込みます。
    MaterialAsset(const std::string& filePath);

    // 追加の解放処理は持たないため、既定のデストラクタを使用します。
    ~MaterialAsset() = default;

    // JSON ファイルからマテリアル情報を読み込みます。
    void Load();

    // 現在のマテリアル情報を JSON ファイルへ保存します。
    void Save();

    // このマテリアルアセットが参照しているファイルパスを返します。
    const std::string& GetFilePath() const { return m_filePath; }

    // マテリアルの基本色です。
    DirectX::XMFLOAT4 baseColor = { 1.0f, 1.0f, 1.0f, 1.0f };

    // 金属らしさを表す値です。
    float metallic = 0.0f;

    // 表面の粗さを表す値です。
    float roughness = 1.0f;

    // 自己発光の強さを表す値です。
    float emissive = 0.0f;

    // ディフューズ、法線、メタリック・ラフネス、エミッシブ用のテクスチャパスです。
    std::string diffuseTexturePath;
    std::string normalTexturePath;
    std::string metallicRoughnessTexturePath;
    std::string emissiveTexturePath;

    // 使用するシェーダの識別子です。
    // 1 は PBR 用の既定値です。
    int shaderId = 1;

    // 透明表現の種類です。
    // 0: 不透明、1: マスク、2: ブレンド。
    int alphaMode = 0;

private:
    // 読み書き対象となるマテリアルファイルのパスです。
    std::string m_filePath;
};
