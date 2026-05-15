#include "TerrainAsset.h"
#include <FastNoiseLite.h>
#include <algorithm>
#include <cmath>

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

    return h * heightScale;
}
