#pragma once

#include <string>
#include <memory>
#include <array>
#include <DirectXMath.h>
#include <d3d11.h>
#include <wrl/client.h>
#include <nlohmann/json.hpp>

// 前方宣言
struct RenderContext;

// --------------------------------------------------------
// レンダリング設定
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

    float maskIntensity = 1.0f;    // マスクの明るさ倍率 (1.0で等倍、上げると白が強くなる)
    float maskContrast = 1.0f;     // マスクの境界の鋭さ (1.0で標準、上げるとパキッとする)

    // [Block 4] Dissolve Color
    DirectX::XMFLOAT3 dissolveEdgeColor = { 1.0f, 0.5f, 0.2f };
    float _padding3 = 0.0f;

    // ★追加 [Block 5] Texture Routing Indices (16 bytes)
    // -1 = 無効 (None), 0~5 = スロット番号
    int mainTexIndex = 0;       // 基本色 (Default: Slot 0)
    int distortionTexIndex = -1;// 歪み   (Default: None)
    int dissolveTexIndex = -1;  // 溶解   (Default: None)
    int maskTexIndex = -1;

    // ★追加 [Block 6] Fresnel Params (16 bytes)
    DirectX::XMFLOAT3 fresnelColor = { 0.0f, 0.5f, 1.0f }; // 発光色
    float fresnelPower = 0.0f;                             // 強度 (0なら無効)

    float flipbookWidth = 1.0f;  // 横のコマ数 (例: 4.0)
    float flipbookHeight = 1.0f; // 縦のコマ数 (例: 4.0)
    float flipbookSpeed = 0.0f;  // 再生速度 (FPSではない。1.0で1秒に1周など調整)
 
    float gradientStrength = 1.0f; // グラデーションマップの強度

    // ★追加 [Block 8] Extra Texture Indices (16 bytes aligned)
    // 構造体のサイズが変わるため、ここに追加
    int gradientTexIndex = -1;  // グラデーションマップ用スロット
    float wpoStrength = 0.0f;  // 変形の強さ (0.0なら無効)
    float wpoSpeed = 1.0f;     // 揺れる速さ
    float wpoFrequency = 1.0f; // 波の細かさ

    // ★追加 [Block 9] (新しいブロックを作成)
    float chromaticAberrationStrength = 0.0f; // ずらす強さ (0.001~0.01くらいが実用的)
   
    // ★追加: paddingを埋める
    float dissolveGlowIntensity = 0.0f; // 発光強度 (0.0ならOFF)
    float dissolveGlowRange = 0.1f;     // 光る幅 (0.001 ~ 0.5)
  
    int uvScrollMode = 0;

    // ★追加 [Block 10] Glow Color (16 bytes aligned)
    DirectX::XMFLOAT3 dissolveGlowColor = { 1.0f, 0.5f, 0.0f }; // デフォルトはオレンジ

    int matCapTexIndex = -1;

    // ★追加 [Block 11] MatCap Params (16 bytes aligned)
    float matCapStrength = 1.0f; // 質感の強さ (0.0=無効, 1.0=完全適用)
    float matCapBlend = 0.5f;    // ブレンド率 (0.0=加算, 1.0=乗算 ... などを好みで調整。今回はシンプルに乗算強度として使います)
    float clipSoftness=0.01f;      // ★変更: float _padding11_2; から変更
    float startEndFadeWidth=0.0f; // ★変更: float _padding11_3; から変更


    // ★追加 [Block 12] MatCap Color (色味補正用)
    DirectX::XMFLOAT3 matCapColor = { 1.0f, 1.0f, 1.0f }; // 白ならテクスチャそのまま
 
    // ★paddingを消費して追加
    int normalTexIndex = -1;    

    // ★追加 [Block 13] Normal Strength (16 bytes aligned)
    float normalStrength = 1.0f; // 凹凸の強さ
    float _padding13_1 = 0.0f;
    float _padding13_2 = 0.0f;
    float _padding13_3 = 0.0f;

    // [Block 14] Flow Map Parameters
    int flowTexIndex = -1;      // フローマップ画像の番号
    float flowStrength = 0.1f;  // 歪みの強さ（移動量）
    float flowSpeed = 1.0f;     // 流れる速さ（サイクルの速さ）
    float sideFadeWidth = 0.0f;

    // [Block 15]
    float visibility = 1.0f;    // 1.0で全表示、0.0で完全透明
    float clipStart = 0.0f;
    float clipEnd = 1.0f;
    float _padding15 = 0.0f;

    // --------------------------------------------------------
    // ★追加 [Block 16] Sub Texture Params (16 bytes aligned)
    // --------------------------------------------------------
    int subTexIndex = -1;      // サブテクスチャのスロット番号 (0~9)
    int subBlendMode = 0;      // 0:乗算, 1:加算, 2:減算, 3:AlphaMask
    DirectX::XMFLOAT2 subUvScrollSpeed = { 0.0f, 0.0f }; // スクロール速度

    // ★追加 [Block 17] Sub Texture Strength (16 bytes aligned)
    // 今後カーブで制御したい場合のために、強度やパディングを用意しておきます
    float subTexStrength = 1.0f; // 影響度
    float subUvRotationSpeed = 0.0f;
    float usePolarCoords = 0.0f;
   
    float toonThreshold = 0.5f; // ★追加 (0.0f:無効, >0.0f:有効) ※旧 _padding17_3

    // ★追加 [Block 18] (16 bytes aligned)
    // パディングが足りなくなったので新しいブロックを作ります
    float toonSmoothing = 0.01f; // 境界の滑らかさ (ジャギー防止用)
    float toonSteps = 0.0f;

    float toonNoiseStrength = 0.0f; // ノイズの強さ (0.0: 無効)
    float toonNoiseSpeed = 0.5f;    // ノイズが流れる速さ

    DirectX::XMFLOAT4 toonShadowColor = { 0.0f, 0.0f, 0.0f, 0.0f };

    int toonNoiseTexIndex = -1;  // ノイズに使うテクスチャのスロット番号 (0~9)
    float _padding20_1 = 0.0f;
    float _padding20_2 = 0.0f;
    float _padding20_3 = 0.0f;
    DirectX::XMFLOAT4 toonSpecular = { 0.0f, 0.0f, 0.0f, 0.0f };

    int toonRampTexIndex = -1;  // ランプテクスチャのスロット番号 (0~9)
    float _padding22_1 = 0.0f;
    float _padding22_2 = 0.0f;
    float _padding22_3 = 0.0f;

};



// --------------------------------------------------------
// エフェクトマテリアルクラス
// --------------------------------------------------------
class EffectMaterial
{
public:
    EffectMaterial();
    ~EffectMaterial() = default;

    static const int TEXTURE_SLOT_COUNT = 10;

    // ★修正: 引数を3つ（slot, path, texture）に変更
    void SetTexture(int slot, const std::string& path, Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> texture);

    // ★追加: 保存やエディタ表示のためにパスを取得する関数
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

    // テクスチャ実体 (GPUリソース)
    std::array<Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>, TEXTURE_SLOT_COUNT> textures;

    // ★追加: テクスチャのファイルパスを記憶する配列
    std::array<std::string, TEXTURE_SLOT_COUNT> texturePaths;
};


