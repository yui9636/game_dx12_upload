#include "GrassComponent.h"

void GrassComponent::EnsureDefaultLayers()
{
    if (!layers.empty()) return;

    // Layer 0: Grass (zassou2 - wider clover-like plant)
    {
        FoliageLayer l;
        l.name = "Grass";
        l.meshPath = "Data/Model/terrain/grass/zassou2.gltf";
        l.splatChannel = 0;
        l.densityMultiplier = 1.0f;
        l.maxPerCell = 3;
        l.densityThreshold = 0.10f;
        l.sizeScale = 1.0f;
        l.maxSlopeDegrees = 60.0f;
        l.useWind = true;
        l.windStrength = 0.35f;
        l.windSpeed = 1.4f;
        l.seed = 1111;
        layers.push_back(l);
    }
    // Layer 1: Weeds (zassou1 - secondary plant variation)
    {
        FoliageLayer l;
        l.name = "Weeds";
        l.meshPath = "Data/Model/terrain/grass/zassou1.gltf";
        l.splatChannel = 0;
        l.densityMultiplier = 0.5f;
        l.maxPerCell = 1;
        l.densityThreshold = 0.35f;
        l.sizeScale = 0.8f;
        l.maxSlopeDegrees = 50.0f;
        l.minAltitudeNorm = 0.0f;
        l.maxAltitudeNorm = 0.7f;
        l.useWind = true;
        l.windStrength = 0.25f;
        l.windSpeed = 1.1f;
        l.colorBottom = { 0.20f, 0.30f, 0.10f };
        l.colorTop    = { 0.60f, 0.75f, 0.30f };
        l.seed = 2222;
        layers.push_back(l);
    }
    // Layer 2: Tall blades (zassou3 - third variation)
    {
        FoliageLayer l;
        l.name = "Tall";
        l.meshPath = "Data/Model/terrain/grass/zassou3.gltf";
        l.splatChannel = 0;
        l.densityMultiplier = 0.3f;
        l.maxPerCell = 1;
        l.densityThreshold = 0.55f;
        l.sizeScale = 1.4f;
        l.maxSlopeDegrees = 45.0f;
        l.useWind = true;
        l.windStrength = 0.45f;
        l.windSpeed = 1.6f;
        l.colorBottom = { 0.14f, 0.24f, 0.08f };
        l.colorTop    = { 0.50f, 0.80f, 0.28f };
        l.seed = 3333;
        layers.push_back(l);
    }
}
