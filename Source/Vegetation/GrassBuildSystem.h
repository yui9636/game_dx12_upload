#pragma once
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>
#include <DirectXMath.h>
#include "Entity/Entity.h"

class Registry;
class IBuffer;
class ITexture;
class Model;

// One instance of an individual foliage object (matches HLSL per-instance layout).
struct GrassInstanceData {
    DirectX::XMFLOAT3 worldPos;     // 12
    float             scale;        //  4
    DirectX::XMFLOAT3 colorTint;    // 12
    float             rotationY;    //  4
};
static_assert(sizeof(GrassInstanceData) == 32, "GrassInstanceData must be 32 bytes");

// Runtime data for one foliage layer (one model + its instance buffer).
struct FoliageLayerRuntime {
    std::shared_ptr<Model> model;
    std::string loadedMeshPath;
    IBuffer*  meshVertexBuffer = nullptr;
    IBuffer*  meshIndexBuffer  = nullptr;
    uint32_t  meshIndexCount   = 0;
    uint32_t  meshVertexStride = 0;
    ITexture* albedoTexture    = nullptr;
    float     alphaCutoff      = 0.5f;
    DirectX::XMFLOAT3 meshLocalMin = { 0.0f, 0.0f, 0.0f };
    DirectX::XMFLOAT3 meshLocalMax = { 1.0f, 1.0f, 1.0f };

    std::unique_ptr<IBuffer> instanceBuffer;
    uint32_t instanceCount = 0;

    // Per-layer parameters passed to the shader (copied at extract time from FoliageLayer).
    DirectX::XMFLOAT3 colorBottom = { 0.16f, 0.28f, 0.10f };
    DirectX::XMFLOAT3 colorTop    = { 0.55f, 0.78f, 0.30f };
    bool   useWind     = true;
    float  windStrength = 0.35f;
    float  windSpeed    = 1.4f;
};

struct GrassRuntimeData {
    std::vector<FoliageLayerRuntime> layers;

    DirectX::XMFLOAT3 boundsCenter  = { 0.0f, 0.0f, 0.0f };
    DirectX::XMFLOAT3 boundsExtents = { 0.0f, 0.0f, 0.0f };

    // Entity-wide settings.
    DirectX::XMFLOAT3 windDirection = { 1.0f, 0.0f, 0.0f };
    float drawDistance = 250.0f;
};

class GrassBuildSystem {
public:
    void Update(Registry& registry);
    const GrassRuntimeData* GetRuntimeData(EntityID entity) const;
    void Clear();

private:
    bool RebuildEntity(EntityID entity, Registry& registry);
    std::unordered_map<EntityID, GrassRuntimeData> m_runtimeDataMap;
};
