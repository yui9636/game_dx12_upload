#pragma once
#include <vector>
#include <string>
#include <cstdint>
#include <DirectXMath.h>

inline constexpr float kDefaultTerrainWorldSize = 1280.0f;
inline constexpr float kLegacyDefaultTerrainWorldSize = 512.0f;

// テクスチャレイヤー定義 (スプラットマップの各チャネルに対応)
struct TerrainLayer {
    std::string albedoPath;
    std::string normalPath;
    std::string roughnessPath;
    float tileScale      = 4.0f;
    float blendSharpness = 1.0f;
};

struct TerrainAutoSplatParams {
    float rockAltitudeMin = 0.62f;
    float rockSlopeDegrees = 32.0f;
    float dirtMidAltitude = 0.45f;
    float dirtStrength = 0.35f;
};

struct TerrainWaterParams {
    bool enabled = true;
    float seaLevel = 4.0f;
    DirectX::XMFLOAT4 shallowColor = { 0.12f, 0.44f, 0.56f, 0.55f };
    DirectX::XMFLOAT4 deepColor = { 0.015f, 0.11f, 0.24f, 0.68f };
    float depthFade = 5.0f;
    float waveSpeed = 0.45f;
    float waveScale = 0.035f;
};

// 地形データ全体を保持するアセット。
// heightData は 0〜1 の float で normalize されたハイトマップ。
// splatData は RGBA 各チャンネルがレイヤーウェイト (4レイヤーまで)。
struct TerrainAsset {
    uint32_t resolution  = 512;
    float    worldSizeX  = kDefaultTerrainWorldSize;
    float    worldSizeZ  = kDefaultTerrainWorldSize;
    float    heightScale = 64.0f;

    uint32_t chunkCountX = 8;
    uint32_t chunkCountZ = 8;

    // CPU側ハイトマップ (resolution * resolution 個の float, 0〜1)
    std::vector<float>   heightData;

    // スプラットマップ (resolution * resolution * 4 バイト, RGBA)
    std::vector<uint8_t> splatData;

    // テクスチャレイヤー (最大4枚)
    std::vector<TerrainLayer> layers;

    // FastNoiseLite 生成パラメータ
    int   noiseType  = 0;
    float noiseFreq  = 0.005f;
    int   octaves    = 4;
    float lacunarity = 2.0f;
    float gain       = 0.5f;
    int   seed       = 1337;

    TerrainAutoSplatParams autoSplat;
    TerrainWaterParams water;

    // レイヤー未設定の Terrain に、草・土・岩の既定レイヤーを補う。
    void EnsureDefaultLayers();

    // FastNoiseLite でハイトデータを生成する。
    void GenerateFromNoise();

    // 高さと傾斜から草・土・岩のスプラットマップを自動生成する。
    void GenerateAutoSplat(const TerrainAutoSplatParams& params);

    // 現在のハイトマップから、池として見える水位を推定する。
    float SuggestVisibleWaterLevel() const;

    // 初期作成時に扱いやすい水面パラメータを設定する。
    void ApplyNaturalWaterPreset();

    // 新規地形で水面がすぐ見える状態にする。
    void SetupDefaultWater();

    // 全ピクセルを同じ高さでリセットする。
    void Reset(float height = 0.0f);

    // heightData から worldSizeX/Z を考慮した Y 値を返す。
    float SampleHeight(float normX, float normZ) const;

    // 旧テンプレートで作られた 512x512 の地形を現在の初期サイズへ移行する。
    bool UpgradeLegacyDefaultWorldSize();
};
