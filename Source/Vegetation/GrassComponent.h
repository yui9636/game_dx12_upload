#pragma once
#include <DirectXMath.h>
#include <cstdint>
#include <string>
#include <vector>
#include <cereal/cereal.hpp>
#include <cereal/types/vector.hpp>
#include <cereal/types/string.hpp>

// One foliage layer (= one model type with its own placement rules).
// A terrain entity holds N layers (grass, weeds, rocks, ...) inside its GrassComponent.
struct FoliageLayer {
    bool        enabled = true;
    std::string name = "Layer";                                       // shown in UI
    std::string meshPath = "Data/Model/terrain/grass/zassou2.gltf";

    // Density (splat channel weight * densityMultiplier * maxPerCell = blades).
    float    densityMultiplier  = 1.0f;
    uint32_t maxPerCell         = 4;
    float    densityThreshold   = 0.10f;

    // Which splat channel drives placement. 0=R(grass), 1=G(dirt), 2=B(rock), -1=any.
    int      splatChannel = 0;

    // Mask by terrain altitude (normalized 0..1 of full heightScale range).
    float    minAltitudeNorm = 0.0f;
    float    maxAltitudeNorm = 1.0f;

    // Mask by terrain slope. 90deg = any slope OK.
    float    maxSlopeDegrees = 90.0f;

    // Object size (in world meters; refers to the largest authored dimension).
    float    sizeScale     = 1.2f;
    float    sizeVariance  = 0.35f;

    // Color gradient applied per vertex (root -> tip) and per-instance random tint.
    DirectX::XMFLOAT3 colorBottom   = { 0.16f, 0.28f, 0.10f };
    DirectX::XMFLOAT3 colorTop      = { 0.55f, 0.78f, 0.30f };
    DirectX::XMFLOAT3 tintVariance  = { 0.10f, 0.10f, 0.05f };

    // Wind animation - disable for rigid props (rocks, fallen logs).
    bool     useWind      = true;
    float    windStrength = 0.35f;
    float    windSpeed    = 1.4f;

    // RNG seed (deterministic regeneration).
    int      seed = 1234;

    // Debug / status (populated by build system).
    uint32_t lastInstanceCount = 0;

    template<class Archive>
    void serialize(Archive& ar) {
        ar( CEREAL_NVP(enabled), CEREAL_NVP(name), CEREAL_NVP(meshPath),
            CEREAL_NVP(densityMultiplier), CEREAL_NVP(maxPerCell),
            CEREAL_NVP(densityThreshold), CEREAL_NVP(splatChannel),
            CEREAL_NVP(minAltitudeNorm), CEREAL_NVP(maxAltitudeNorm),
            CEREAL_NVP(maxSlopeDegrees),
            CEREAL_NVP(sizeScale), CEREAL_NVP(sizeVariance),
            cereal::make_nvp("cbR", colorBottom.x),
            cereal::make_nvp("cbG", colorBottom.y),
            cereal::make_nvp("cbB", colorBottom.z),
            cereal::make_nvp("ctR", colorTop.x),
            cereal::make_nvp("ctG", colorTop.y),
            cereal::make_nvp("ctB", colorTop.z),
            cereal::make_nvp("tvR", tintVariance.x),
            cereal::make_nvp("tvG", tintVariance.y),
            cereal::make_nvp("tvB", tintVariance.z),
            CEREAL_NVP(useWind), CEREAL_NVP(windStrength), CEREAL_NVP(windSpeed),
            CEREAL_NVP(seed) );
    }
};

// Per-terrain foliage container. The component itself just holds the layer list
// + entity-wide settings (wind direction, draw distance, etc).
struct GrassComponent {
    bool enabled = true;
    bool needsRebuild = true;

    std::vector<FoliageLayer> layers;

    // Global wind direction shared by all layers (each layer scales by its own strength).
    DirectX::XMFLOAT3 windDirection = { 1.0f, 0.0f, 0.6f };

    // Distance cull (any layer beyond this from camera is skipped).
    float drawDistance = 250.0f;

    bool showInEditor = true;

    // Convenience helper: populate three default layers covering grass/weeds/rocks.
    void EnsureDefaultLayers();

    template<class Archive>
    void serialize(Archive& ar) {
        ar( CEREAL_NVP(enabled), CEREAL_NVP(layers),
            cereal::make_nvp("windX", windDirection.x),
            cereal::make_nvp("windY", windDirection.y),
            cereal::make_nvp("windZ", windDirection.z),
            CEREAL_NVP(drawDistance), CEREAL_NVP(showInEditor) );
    }
};
