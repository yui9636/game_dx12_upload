#include "TerrainAsset.h"
#include "TerrainGpuPipeline.h"
#include <algorithm>
#include <cmath>

namespace
{
    float Clamp01(float v) { return std::clamp(v, 0.0f, 1.0f); }
    float SmoothStep(float edge0, float edge1, float x)
    {
        if (std::abs(edge1 - edge0) < 0.0001f) return x >= edge1 ? 1.0f : 0.0f;
        const float t = std::clamp((x - edge0) / (edge1 - edge0), 0.0f, 1.0f);
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
    // GPU compute. Falls back to a flat heightmap if GPU init fails.
    if (!TerrainGpuPipeline::Instance().Run(*this, TerrainGpuPipeline::StageNoise)) {
        heightData.assign(static_cast<size_t>(resolution) * resolution, 0.5f);
    }
    if (splatData.size() != static_cast<size_t>(resolution) * resolution * 4u) {
        splatData.assign(static_cast<size_t>(resolution) * resolution * 4u, 0);
        for (uint32_t i = 0; i < resolution * resolution; ++i) {
            splatData[i * 4u + 0] = 255;  // default grass everywhere
        }
    }
}

void TerrainAsset::GenerateAutoSplat(const TerrainAutoSplatParams& params)
{
    autoSplat = params;
    // GPU compute. Existing heightData is uploaded; splatData is regenerated.
    if (!TerrainGpuPipeline::Instance().Run(*this, TerrainGpuPipeline::StageAutoSplat)) {
        const size_t cells = static_cast<size_t>(resolution) * resolution;
        splatData.assign(cells * 4u, 0);
        for (size_t i = 0; i < cells; ++i) splatData[i * 4u + 0] = 255;
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
        : (minHeight + (maxHeight - minHeight) * 0.45f);

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

    int ix1 = (std::min)(ix + 1, static_cast<int>(resolution) - 1);
    int iz1 = (std::min)(iz + 1, static_cast<int>(resolution) - 1);

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

// ---------------------------------------------------------------------------
// Phase 1.5: Hydraulic Erosion (GPU compute via TerrainGpuPipeline)
// ---------------------------------------------------------------------------

void TerrainAsset::RunHydraulicErosion(const TerrainErosionParams& p)
{
    erosion = p;
    // Erode existing heightData on GPU and refresh splat in the same dispatch.
    TerrainGpuPipeline::Instance().Run(
        *this,
        TerrainGpuPipeline::StageErode | TerrainGpuPipeline::StageAutoSplat);
}
