#pragma once
#include <vector>
#include <string>
#include <cstdint>
#include <DirectXMath.h>
#include <cereal/cereal.hpp>
#include <cereal/types/vector.hpp>
#include <cereal/types/string.hpp>

inline constexpr float kDefaultTerrainWorldSize = 1280.0f;
inline constexpr float kLegacyDefaultTerrainWorldSize = 512.0f;

// テクスチャレイヤー定義 (スプラットマップの各チャネルに対応)
struct TerrainLayer {
    std::string albedoPath;
    std::string normalPath;
    std::string roughnessPath;
    float tileScale      = 4.0f;
    float blendSharpness = 1.0f;

    template<class Archive>
    void serialize(Archive& ar) {
        ar( CEREAL_NVP(albedoPath), CEREAL_NVP(normalPath),
            CEREAL_NVP(roughnessPath), CEREAL_NVP(tileScale),
            CEREAL_NVP(blendSharpness) );
    }
};

struct TerrainAutoSplatParams {
    float rockAltitudeMin = 0.62f;
    float rockSlopeDegrees = 32.0f;
    float dirtMidAltitude = 0.45f;
    float dirtStrength = 0.35f;

    template<class Archive>
    void serialize(Archive& ar) {
        ar( CEREAL_NVP(rockAltitudeMin), CEREAL_NVP(rockSlopeDegrees),
            CEREAL_NVP(dirtMidAltitude), CEREAL_NVP(dirtStrength) );
    }
};

struct TerrainWaterParams {
    bool enabled = true;
    float seaLevel = 4.0f;
    DirectX::XMFLOAT4 shallowColor = { 0.12f, 0.44f, 0.56f, 0.55f };
    DirectX::XMFLOAT4 deepColor = { 0.015f, 0.11f, 0.24f, 0.68f };
    float depthFade = 5.0f;
    float waveSpeed = 0.45f;
    float waveScale = 0.035f;

    template<class Archive>
    void serialize(Archive& ar) {
        ar( CEREAL_NVP(enabled), CEREAL_NVP(seaLevel),
            cereal::make_nvp("shallowR", shallowColor.x),
            cereal::make_nvp("shallowG", shallowColor.y),
            cereal::make_nvp("shallowB", shallowColor.z),
            cereal::make_nvp("shallowA", shallowColor.w),
            cereal::make_nvp("deepR", deepColor.x),
            cereal::make_nvp("deepG", deepColor.y),
            cereal::make_nvp("deepB", deepColor.z),
            cereal::make_nvp("deepA", deepColor.w),
            CEREAL_NVP(depthFade), CEREAL_NVP(waveSpeed), CEREAL_NVP(waveScale) );
    }
};

// Parameters for droplet-based hydraulic erosion. Produces natural valleys,
// ridges and basins out of the procedural noise.
struct TerrainErosionParams {
    int   iterations            = 60000;
    int   erosionRadius         = 3;
    int   maxDropletLifetime    = 30;
    float inertia               = 0.05f;
    float sedimentCapacityFactor= 4.0f;
    float minSedimentCapacity   = 0.01f;
    float erodeSpeed            = 0.30f;
    float depositSpeed          = 0.30f;
    float evaporateSpeed        = 0.01f;
    float gravity               = 4.0f;
    float initialWater          = 1.0f;
    float initialSpeed          = 1.0f;
    int   seed                  = 12345;

    template<class Archive>
    void serialize(Archive& ar) {
        ar( CEREAL_NVP(iterations), CEREAL_NVP(erosionRadius),
            CEREAL_NVP(maxDropletLifetime), CEREAL_NVP(inertia),
            CEREAL_NVP(sedimentCapacityFactor), CEREAL_NVP(minSedimentCapacity),
            CEREAL_NVP(erodeSpeed), CEREAL_NVP(depositSpeed),
            CEREAL_NVP(evaporateSpeed), CEREAL_NVP(gravity),
            CEREAL_NVP(initialWater), CEREAL_NVP(initialSpeed), CEREAL_NVP(seed) );
    }
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

    // ノイズ生成パラメータ
    // noiseType: 0 = Hybrid (Simplex + Cellular), 1 = Simplex only, 2 = Cellular only
    int   noiseType  = 0;
    float noiseFreq  = 0.005f;
    int   octaves    = 4;
    float lacunarity = 2.0f;
    float gain       = 0.5f;
    int   seed       = 1337;

    // Multi-scale domain warp strength (in meters). Eliminates the underlying
    // hex/grid structure of the noise. 0 = no warp, ~150 = strong organic flow.
    float domainWarpStrength = 80.0f;

    // Terrace/plateau quantization. 0 = off, otherwise number of steps the
    // height is snapped to (with a smooth blend). Good for mesa landscapes.
    float terraceSteps = 0.0f;

    TerrainAutoSplatParams autoSplat;
    TerrainWaterParams water;
    TerrainErosionParams erosion;

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

    // Phase 1.5: hydraulic erosion. GPU compute via TerrainGpuPipeline.
    // Modifies heightData in place and refreshes splatData via auto-splat.
    void RunHydraulicErosion(const TerrainErosionParams& params);

    template<class Archive>
    void serialize(Archive& ar) {
        ar( CEREAL_NVP(resolution), CEREAL_NVP(worldSizeX), CEREAL_NVP(worldSizeZ),
            CEREAL_NVP(heightScale), CEREAL_NVP(chunkCountX), CEREAL_NVP(chunkCountZ),
            CEREAL_NVP(heightData), CEREAL_NVP(splatData), CEREAL_NVP(layers),
            CEREAL_NVP(noiseType), CEREAL_NVP(noiseFreq), CEREAL_NVP(octaves),
            CEREAL_NVP(lacunarity), CEREAL_NVP(gain), CEREAL_NVP(seed),
            CEREAL_NVP(domainWarpStrength), CEREAL_NVP(terraceSteps),
            CEREAL_NVP(autoSplat), CEREAL_NVP(water), CEREAL_NVP(erosion) );
    }
};
