#pragma once
#include <string>
#include <DirectXMath.h>
#include "Utils/JSONManager.h"

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

    // ─── Toon シェーダー専用パラメータ (shaderId == 2 のときのみ使用) ───
    // 陰影モード: 0=Bands (段階量子化), 1=Three-Tier (lit/mid/deep), 2=Ramp (1D テクスチャ)
    int toonShadingMode = 0;
    // 半影色 (Three-Tier の中間色 / Bands 時の影 Tint)。
    DirectX::XMFLOAT3 toonShadowTint = { 0.55f, 0.50f, 0.65f };
    // 深影色 (Three-Tier の最も暗い帯)。
    DirectX::XMFLOAT3 toonShadowDeep = { 0.25f, 0.22f, 0.38f };
    // Three-Tier の閾値。NdotL がこの値以上で lit、midThreshold〜deepThreshold で mid、それ未満で deep。
    float toonShadowMidThreshold  = 0.55f;
    float toonShadowDeepThreshold = 0.28f;

    // リムライト。
    DirectX::XMFLOAT3 toonRimColor   = { 1.0f, 1.0f, 1.0f };
    float toonRimPower    = 4.0f;
    float toonRimStrength = 0.4f;

    // 段階数 (Bands モード)。2〜5。
    int  toonBandLevels = 3;
    // ランプテクスチャパス (Ramp モード)。
    std::string toonRampTexturePath;
    // 旧 useRampTexture 互換: 読込時に true なら toonShadingMode=2 に移行
    bool toonUseRampTexture = false;

    // ─── アウトライン ───
    bool toonOutlineEnabled = true;
    DirectX::XMFLOAT3 toonOutlineColor = { 0.05f, 0.05f, 0.08f };
    float toonOutlineWidth = 0.012f;
    // 距離スケール (0=固定幅、1=フル距離スケール)。近距離は太く、遠距離は細くなる。
    float toonOutlineDistanceScale = 0.0f;

    // ─── Banded Specular (アニメ風ハードハイライト) ───
    DirectX::XMFLOAT3 toonSpecColor = { 1.0f, 1.0f, 1.0f };
    // 強度 (0=無効、推奨 0.5〜1.0)。
    float toonSpecStrength = 0.0f;
    // 鋭さ (0〜1、大きいほど狭く鋭い)。
    float toonSpecSharpness = 0.5f;
    // 段差閾値 (specular がこの値以上なら 1、未満なら 0)。
    float toonSpecThreshold = 0.5f;

    // ─── キャラ固定ライト方向 ───
    bool toonUseFixedLight = false;
    // 固定光方向 (キャラから見た光源方向の負ベクトル)。前上方推奨。
    DirectX::XMFLOAT3 toonFixedLightDir = { 0.0f, -0.5f, -0.7f };

    // ─── 異方性ハイライト (Kajiya-Kay 簡易版、髪マテリアル向け) ───
    bool toonUseAnisotropic = false;
    // 鋭さ (0〜1)。
    float toonAnisoSharpness = 0.5f;
    // 縦方向ハイライト位置オフセット (-1〜1)。
    float toonAnisoOffset = 0.0f;

private:
    // 読み書き対象となるマテリアルファイルのパスです。
    std::string m_filePath;
};
