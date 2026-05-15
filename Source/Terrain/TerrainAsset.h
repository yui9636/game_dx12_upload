#pragma once
#include <vector>
#include <string>
#include <cstdint>

// テクスチャレイヤー定義 (スプラットマップの各チャネルに対応)
struct TerrainLayer {
    std::string albedoPath;
    std::string normalPath;
    std::string roughnessPath;
    float tileScale      = 4.0f;
    float blendSharpness = 1.0f;
};

// 地形データ全体を保持するアセット。
// heightData は 0〜1 の float で normalize されたハイトマップ。
// splatData は RGBA 各チャンネルがレイヤーウェイト (4レイヤーまで)。
struct TerrainAsset {
    uint32_t resolution  = 512;
    float    worldSizeX  = 512.0f;
    float    worldSizeZ  = 512.0f;
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

    // FastNoiseLite でハイトデータを生成する。
    void GenerateFromNoise();

    // 全ピクセルを同じ高さでリセットする。
    void Reset(float height = 0.0f);

    // heightData から worldSizeX/Z を考慮した Y 値を返す。
    float SampleHeight(float normX, float normZ) const;
};
