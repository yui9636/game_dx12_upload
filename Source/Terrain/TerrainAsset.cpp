#include "TerrainAsset.h"
#include <FastNoiseLite.h>
#include <algorithm>
#include <cmath>

namespace
{
    float Clamp01(float v)
    {
        return std::clamp(v, 0.0f, 1.0f);
    }

    float Lerp(float a, float b, float t)
    {
        return a + (b - a) * t;
    }

    float SmoothStep(float edge0, float edge1, float x)
    {
        if (std::abs(edge1 - edge0) < 0.0001f) {
            return x >= edge1 ? 1.0f : 0.0f;
        }
        float t = std::clamp((x - edge0) / (edge1 - edge0), 0.0f, 1.0f);
        return t * t * (3.0f - 2.0f * t);
    }

    float Noise01(FastNoiseLite& noise, float x, float z)
    {
        return noise.GetNoise(x, z) * 0.5f + 0.5f;
    }

    void SmoothHeightField(std::vector<float>& data, uint32_t resolution, float strength)
    {
        if (resolution < 3 || data.empty() || strength <= 0.0f) {
            return;
        }

        std::vector<float> src = data;
        for (uint32_t z = 0; z < resolution; ++z) {
            for (uint32_t x = 0; x < resolution; ++x) {
                float sum = 0.0f;
                float weightSum = 0.0f;
                for (int dz = -1; dz <= 1; ++dz) {
                    for (int dx = -1; dx <= 1; ++dx) {
                        const int sx = std::clamp<int>(static_cast<int>(x) + dx, 0, static_cast<int>(resolution) - 1);
                        const int sz = std::clamp<int>(static_cast<int>(z) + dz, 0, static_cast<int>(resolution) - 1);
                        const float w = (dx == 0 && dz == 0) ? 4.0f : ((dx == 0 || dz == 0) ? 2.0f : 1.0f);
                        sum += src[static_cast<size_t>(sz) * resolution + sx] * w;
                        weightSum += w;
                    }
                }
                const size_t index = static_cast<size_t>(z) * resolution + x;
                data[index] = Lerp(src[index], sum / weightSum, strength);
            }
        }
    }

    void NormalizeHeightField(std::vector<float>& data)
    {
        if (data.empty()) {
            return;
        }

        std::vector<float> sorted = data;
        std::sort(sorted.begin(), sorted.end());
        const size_t lowIndex = static_cast<size_t>(static_cast<float>(sorted.size() - 1) * 0.02f);
        const size_t highIndex = static_cast<size_t>(static_cast<float>(sorted.size() - 1) * 0.985f);
        const float low = sorted[lowIndex];
        const float high = sorted[(std::max)(highIndex, lowIndex + 1u)];
        const float range = (std::max)(high - low, 0.0001f);

        for (float& h : data) {
            h = Clamp01((h - low) / range);
        }
    }
}

void TerrainAsset::EnsureDefaultLayers()
{
    if (!layers.empty()) {
        return;
    }

    layers.resize(3);
    layers[0].albedoPath = "Data/Model/terrain/wispy-grass-meadow_albedo.png";
    layers[0].tileScale  = 7.0f;
    layers[1].albedoPath = "Data/Model/terrain/rocky_dirt1-albedo.png";
    layers[1].normalPath = "Data/Model/terrain/rocky_terrain_02_nor_gl_4k.exr";
    layers[1].tileScale  = 5.5f;
    layers[2].albedoPath = "Data/Model/terrain/rock_boulder_cracked_diff_4k.jpg";
    layers[2].normalPath = "Data/Model/terrain/rock_boulder_cracked_nor_gl_4k.exr";
    layers[2].tileScale  = 3.5f;
}

void TerrainAsset::GenerateFromNoise()
{
    resolution = std::max<uint32_t>(resolution, 16u);
    heightData.assign(static_cast<size_t>(resolution) * resolution, 0.0f);

    auto setupNoise = [&](FastNoiseLite& noise, int seedOffset, float frequencyScale, int octaveBias = 0) {
        noise.SetNoiseType(static_cast<FastNoiseLite::NoiseType>(noiseType));
        noise.SetFrequency((std::max)(noiseFreq * frequencyScale, 0.0001f));
        noise.SetFractalType(FastNoiseLite::FractalType_FBm);
        noise.SetFractalOctaves((std::max)(1, octaves + octaveBias));
        noise.SetFractalLacunarity(lacunarity);
        noise.SetFractalGain(gain);
        noise.SetSeed(seed + seedOffset);
    };

    FastNoiseLite continental;
    FastNoiseLite hills;
    FastNoiseLite ridge;
    FastNoiseLite detail;
    FastNoiseLite basinNoise;
    setupNoise(continental, 0, 0.42f, -1);
    setupNoise(hills, 17, 1.15f, 0);
    setupNoise(ridge, 41, 0.80f, 0);
    setupNoise(detail, 83, 4.50f, 1);
    setupNoise(basinNoise, 131, 0.70f, -1);

    const float inv = 1.0f / static_cast<float>((std::max)(resolution - 1u, 1u));

    for (uint32_t z = 0; z < resolution; ++z) {
        for (uint32_t x = 0; x < resolution; ++x) {
            const float fx = static_cast<float>(x);
            const float fz = static_cast<float>(z);
            const float u = fx * inv;
            const float v = fz * inv;

            const float continent = Noise01(continental, fx, fz);
            const float hill = Noise01(hills, fx, fz) - 0.5f;
            const float ridgeValue = 1.0f - std::abs(ridge.GetNoise(fx, fz));
            const float ridged = std::pow(Clamp01(ridgeValue), 2.2f);
            const float fine = Noise01(detail, fx, fz) - 0.5f;

            const float edgeX = std::abs(u - 0.5f) * 2.0f;
            const float edgeZ = std::abs(v - 0.5f) * 2.0f;
            const float distantRim = SmoothStep(0.42f, 1.0f, (std::max)(edgeX, edgeZ));

            float h = 0.44f;
            h += (continent - 0.5f) * 0.42f;
            h += hill * 0.22f;
            h += ridged * 0.24f;
            h += fine * 0.055f;
            h += distantRim * 0.12f;

            heightData[static_cast<size_t>(z) * resolution + x] = h;
        }
    }

    NormalizeHeightField(heightData);
    SmoothHeightField(heightData, resolution, 0.32f);
    SmoothHeightField(heightData, resolution, 0.18f);

    for (uint32_t z = 0; z < resolution; ++z) {
        for (uint32_t x = 0; x < resolution; ++x) {
            const float u = static_cast<float>(x) * inv;
            const float v = static_cast<float>(z) * inv;
            const float dx = (u - 0.48f) / 0.23f;
            const float dz = (v - 0.53f) / 0.16f;
            const float basinDist = std::sqrt(dx * dx + dz * dz);
            const float basinMask = 1.0f - SmoothStep(0.80f, 1.35f, basinDist);
            const float floorNoise = (Noise01(basinNoise, static_cast<float>(x), static_cast<float>(z)) - 0.5f) * 0.018f;
            const float basinFloor = 0.285f + floorNoise;
            const float shoreRise = SmoothStep(0.45f, 1.22f, basinDist) * 0.16f;
            const float target = basinFloor + shoreRise;

            const size_t index = static_cast<size_t>(z) * resolution + x;
            const float carved = (std::min)(heightData[index], target);
            heightData[index] = Lerp(heightData[index], carved, basinMask * 0.92f);
        }
    }

    SmoothHeightField(heightData, resolution, 0.12f);
    for (float& h : heightData) {
        h = std::clamp(h, 0.035f, 0.965f);
    }

    if (splatData.empty()) {
        splatData.assign(resolution * resolution * 4, 0);
        for (uint32_t i = 0; i < resolution * resolution; ++i) {
            splatData[i * 4 + 0] = 255; // Layer 0 が全面デフォルト
        }
    }
}

void TerrainAsset::GenerateAutoSplat(const TerrainAutoSplatParams& params)
{
    const size_t pixelCount =
        static_cast<size_t>(resolution) * static_cast<size_t>(resolution);
    if (heightData.size() != pixelCount) {
        GenerateFromNoise();
    }

    splatData.assign(pixelCount * 4u, 0);
    const float cellSizeX = worldSizeX / static_cast<float>((std::max)(resolution - 1u, 1u));
    const float cellSizeZ = worldSizeZ / static_cast<float>((std::max)(resolution - 1u, 1u));
    const float slopeStart = (std::max)(params.rockSlopeDegrees - 8.0f, 0.0f);
    const float slopeEnd = (std::min)(params.rockSlopeDegrees + 16.0f, 89.0f);

    FastNoiseLite materialNoise;
    materialNoise.SetNoiseType(FastNoiseLite::NoiseType_OpenSimplex2);
    materialNoise.SetFrequency((std::max)(noiseFreq * 3.0f, 0.0001f));
    materialNoise.SetFractalType(FastNoiseLite::FractalType_FBm);
    materialNoise.SetFractalOctaves(3);
    materialNoise.SetFractalLacunarity(2.0f);
    materialNoise.SetFractalGain(0.55f);
    materialNoise.SetSeed(seed + 271);

    const bool useWater = water.enabled;
    const float seaLevelNorm = Clamp01((water.seaLevel + heightScale * 0.5f) / (std::max)(heightScale, 0.0001f));

    for (uint32_t z = 0; z < resolution; ++z) {
        for (uint32_t x = 0; x < resolution; ++x) {
            const uint32_t x0 = (x > 0) ? x - 1 : x;
            const uint32_t x1 = (x + 1 < resolution) ? x + 1 : x;
            const uint32_t z0 = (z > 0) ? z - 1 : z;
            const uint32_t z1 = (z + 1 < resolution) ? z + 1 : z;

            const float h = heightData[static_cast<size_t>(z) * resolution + x];
            const float hL = heightData[static_cast<size_t>(z) * resolution + x0] * heightScale;
            const float hR = heightData[static_cast<size_t>(z) * resolution + x1] * heightScale;
            const float hD = heightData[static_cast<size_t>(z0) * resolution + x] * heightScale;
            const float hU = heightData[static_cast<size_t>(z1) * resolution + x] * heightScale;
            const float dx = (hR - hL) / ((std::max)(static_cast<float>(x1 - x0), 1.0f) * cellSizeX);
            const float dz = (hU - hD) / ((std::max)(static_cast<float>(z1 - z0), 1.0f) * cellSizeZ);
            const float slopeRadians = std::atan(std::sqrt(dx * dx + dz * dz));
            const float slopeDegrees = slopeRadians * 57.2957795f;

            const float materialVar = Noise01(materialNoise, static_cast<float>(x), static_cast<float>(z));
            const float worldY = h * heightScale - heightScale * 0.5f;
            const float waterDelta = useWater ? (worldY - water.seaLevel) : heightScale;
            const float shoreWetness = useWater
                ? (1.0f - SmoothStep(0.5f, (std::max)(heightScale * 0.10f, 5.0f), std::abs(waterDelta)))
                : 0.0f;
            const float underwater = useWater ? (1.0f - SmoothStep(-2.0f, 0.8f, waterDelta)) : 0.0f;

            float rock = SmoothStep(params.rockAltitudeMin, 1.0f, h);
            rock = (std::max)(rock, SmoothStep(slopeStart, slopeEnd, slopeDegrees));
            rock += SmoothStep(0.74f, 1.0f, h + materialVar * 0.08f) * 0.18f;
            rock = std::clamp(rock, 0.0f, 1.0f);

            const float midBand = 1.0f - std::clamp(std::abs(h - params.dirtMidAltitude) / 0.32f, 0.0f, 1.0f);
            float dirt = midBand * params.dirtStrength + SmoothStep(12.0f, params.rockSlopeDegrees, slopeDegrees) * 0.28f;
            dirt += shoreWetness * 0.75f;
            dirt += underwater * 0.55f;
            dirt += SmoothStep(seaLevelNorm - 0.05f, seaLevelNorm + 0.12f, h) * (1.0f - SmoothStep(seaLevelNorm + 0.12f, seaLevelNorm + 0.28f, h)) * 0.18f;
            dirt *= Lerp(0.88f, 1.15f, materialVar);
            dirt *= (1.0f - rock * 0.65f);
            dirt = std::clamp(dirt, 0.0f, 1.0f - rock);

            float grass = (std::max)(0.0f, 1.0f - rock - dirt);
            grass *= (1.0f - underwater * 0.85f);
            const float sum = grass + dirt + rock;
            if (sum > 0.0001f) {
                grass /= sum;
                dirt /= sum;
                rock /= sum;
            } else {
                grass = 1.0f;
                dirt = 0.0f;
                rock = 0.0f;
            }

            const size_t p = (static_cast<size_t>(z) * resolution + x) * 4u;
            splatData[p + 0] = static_cast<uint8_t>(std::clamp(grass * 255.0f, 0.0f, 255.0f));
            splatData[p + 1] = static_cast<uint8_t>(std::clamp(dirt  * 255.0f, 0.0f, 255.0f));
            splatData[p + 2] = static_cast<uint8_t>(std::clamp(rock  * 255.0f, 0.0f, 255.0f));
            splatData[p + 3] = 0;
        }
    }
}

float TerrainAsset::SuggestVisibleWaterLevel() const
{
    if (heightData.empty()) {
        return water.seaLevel;
    }

    std::vector<float> sorted = heightData;
    std::sort(sorted.begin(), sorted.end());
    const float minHeight = sorted.front();
    const float maxHeight = sorted.back();
    if (maxHeight <= minHeight + 0.001f) {
        return minHeight * heightScale - heightScale * 0.40f;
    }

    const size_t p = static_cast<size_t>(static_cast<float>(sorted.size() - 1) * 0.24f);
    const float valleyLevel = sorted[p];
    const float lowClamp = minHeight + 0.035f;
    const float highClamp = (std::min)(0.45f, maxHeight - 0.06f);
    const float levelNorm = (highClamp > lowClamp)
        ? std::clamp(valleyLevel + 0.035f, lowClamp, highClamp)
        : Lerp(minHeight, maxHeight, 0.45f);

    return levelNorm * heightScale - heightScale * 0.5f;
}

void TerrainAsset::ApplyNaturalWaterPreset()
{
    water.shallowColor = { 0.18f, 0.50f, 0.58f, 0.62f };
    water.deepColor = { 0.018f, 0.095f, 0.18f, 0.78f };
    water.depthFade = 9.5f;
    water.waveSpeed = 0.24f;
    water.waveScale = 0.014f;
}

void TerrainAsset::SetupDefaultWater()
{
    water.enabled = true;
    ApplyNaturalWaterPreset();
    water.seaLevel = SuggestVisibleWaterLevel();
}

void TerrainAsset::Reset(float height)
{
    height = std::clamp(height, 0.0f, 1.0f);
    heightData.assign(resolution * resolution, height);
    splatData.assign(resolution * resolution * 4, 0);
    for (uint32_t i = 0; i < resolution * resolution; ++i) {
        splatData[i * 4 + 0] = 255;
    }
}

float TerrainAsset::SampleHeight(float normX, float normZ) const
{
    if (heightData.empty()) return 0.0f;

    normX = std::clamp(normX, 0.0f, 1.0f);
    normZ = std::clamp(normZ, 0.0f, 1.0f);

    float fx = normX * static_cast<float>(resolution - 1);
    float fz = normZ * static_cast<float>(resolution - 1);
    int   ix = static_cast<int>(fx);
    int   iz = static_cast<int>(fz);
    float tx = fx - static_cast<float>(ix);
    float tz = fz - static_cast<float>(iz);

    int ix1 = std::min(ix + 1, static_cast<int>(resolution) - 1);
    int iz1 = std::min(iz + 1, static_cast<int>(resolution) - 1);

    float h00 = heightData[iz  * resolution + ix ];
    float h10 = heightData[iz  * resolution + ix1];
    float h01 = heightData[iz1 * resolution + ix ];
    float h11 = heightData[iz1 * resolution + ix1];

    float h = h00 * (1.0f - tx) * (1.0f - tz)
            + h10 * tx * (1.0f - tz)
            + h01 * (1.0f - tx) * tz
            + h11 * tx * tz;

    return h * heightScale - heightScale * 0.5f;
}

bool TerrainAsset::UpgradeLegacyDefaultWorldSize()
{
    constexpr float kEpsilon = 0.01f;
    const bool legacyX = std::abs(worldSizeX - kLegacyDefaultTerrainWorldSize) <= kEpsilon;
    const bool legacyZ = std::abs(worldSizeZ - kLegacyDefaultTerrainWorldSize) <= kEpsilon;
    if (!legacyX || !legacyZ) {
        return false;
    }

    worldSizeX = kDefaultTerrainWorldSize;
    worldSizeZ = kDefaultTerrainWorldSize;
    return true;
}
