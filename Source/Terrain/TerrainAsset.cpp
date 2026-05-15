#include "TerrainAsset.h"
#include <FastNoiseLite.h>
#include <algorithm>
#include <cmath>

namespace
{
    float SmoothStep(float edge0, float edge1, float x)
    {
        if (std::abs(edge1 - edge0) < 0.0001f) {
            return x >= edge1 ? 1.0f : 0.0f;
        }
        float t = std::clamp((x - edge0) / (edge1 - edge0), 0.0f, 1.0f);
        return t * t * (3.0f - 2.0f * t);
    }
}

void TerrainAsset::EnsureDefaultLayers()
{
    if (!layers.empty()) {
        return;
    }

    layers.resize(3);
    layers[0].albedoPath = "Data/Model/terrain/wispy-grass-meadow_albedo.png";
    layers[0].tileScale  = 8.0f;
    layers[1].albedoPath = "Data/Model/terrain/rocky_dirt1-albedo.png";
    layers[1].tileScale  = 6.0f;
    layers[2].albedoPath = "Data/Model/terrain/layered-rock1-albedo.png";
    layers[2].tileScale  = 4.0f;
}

void TerrainAsset::GenerateFromNoise()
{
    heightData.assign(resolution * resolution, 0.0f);

    FastNoiseLite noise;
    noise.SetNoiseType(static_cast<FastNoiseLite::NoiseType>(noiseType));
    noise.SetFrequency(noiseFreq);
    noise.SetFractalType(FastNoiseLite::FractalType_FBm);
    noise.SetFractalOctaves(octaves);
    noise.SetFractalLacunarity(lacunarity);
    noise.SetFractalGain(gain);
    noise.SetSeed(seed);

    for (uint32_t z = 0; z < resolution; ++z) {
        for (uint32_t x = 0; x < resolution; ++x) {
            float v = noise.GetNoise(static_cast<float>(x), static_cast<float>(z));
            heightData[z * resolution + x] = (v + 1.0f) * 0.5f;
        }
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

            float rock = SmoothStep(params.rockAltitudeMin, 1.0f, h);
            rock = (std::max)(rock, SmoothStep(slopeStart, slopeEnd, slopeDegrees));
            rock = std::clamp(rock, 0.0f, 1.0f);

            const float midBand = 1.0f - std::clamp(std::abs(h - params.dirtMidAltitude) / 0.32f, 0.0f, 1.0f);
            float dirt = midBand * params.dirtStrength + SmoothStep(12.0f, params.rockSlopeDegrees, slopeDegrees) * 0.28f;
            dirt *= (1.0f - rock * 0.65f);
            dirt = std::clamp(dirt, 0.0f, 1.0f - rock);

            float grass = (std::max)(0.0f, 1.0f - rock - dirt);
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

    float minHeight = 1.0f;
    float maxHeight = 0.0f;
    for (float h : heightData) {
        minHeight = std::min(minHeight, h);
        maxHeight = std::max(maxHeight, h);
    }

    const float minY = minHeight * heightScale - heightScale * 0.5f;
    const float maxY = maxHeight * heightScale - heightScale * 0.5f;
    if (maxY <= minY + 0.001f) {
        return minY + heightScale * 0.1f;
    }

    return minY + (maxY - minY) * 0.38f;
}

void TerrainAsset::ApplyNaturalWaterPreset()
{
    water.shallowColor = { 0.12f, 0.44f, 0.56f, 0.55f };
    water.deepColor = { 0.015f, 0.11f, 0.24f, 0.68f };
    water.depthFade = 7.5f;
    water.waveSpeed = 0.32f;
    water.waveScale = 0.022f;
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
