#pragma once

#include <vector>
#include <memory>
#include <cstdint>
#include <cstddef>
#include <functional>
#include <DirectXMath.h>
#include "RenderState.h"
#include "EffectRuntime/EffectGraphAsset.h"

class ModelResource;
class MaterialAsset;
class ITexture;

struct RenderQueueMetrics {
    double meshExtractMs = 0.0;
    double batchSortMs = 0.0;
    uint32_t materialResolveCount = 0;
    uint32_t materialGroupCount = 0;
    uint32_t opaquePacketCount = 0;
    uint32_t transparentPacketCount = 0;
    uint32_t opaqueBatchCount = 0;
    uint32_t maxInstancesPerBatch = 0;
    uint32_t nonSkinnedOpaquePacketCount = 0;
    uint32_t skinnedOpaquePacketCount = 0;
    uint32_t opaquePacketVectorGrowths = 0;
    uint32_t transparentPacketVectorGrowths = 0;
    uint32_t opaqueBatchVectorGrowths = 0;
    uint32_t effectMeshPacketCount = 0;
    uint32_t effectParticlePacketCount = 0;
};

struct DrawBatchKey {
    ModelResource* modelResource = nullptr;
    int shaderId = 1;
    bool castShadow = true;
    BlendState blendState = BlendState::Opaque;
    DepthState depthState = DepthState::TestAndWrite;
    RasterizerState rasterizerState = RasterizerState::SolidCullBack;
    DirectX::XMFLOAT4 baseColor = { 1.0f, 1.0f, 1.0f, 1.0f };
    float metallic = 0.0f;
    float roughness = 1.0f;
    float emissive = 0.0f;
    MaterialAsset* materialAsset = nullptr;
    uint64_t materialGroupHash = 0;

    bool operator==(const DrawBatchKey& other) const {
        return modelResource == other.modelResource
            && shaderId == other.shaderId
            && castShadow == other.castShadow
            && blendState == other.blendState
            && depthState == other.depthState
            && rasterizerState == other.rasterizerState
            && baseColor.x == other.baseColor.x
            && baseColor.y == other.baseColor.y
            && baseColor.z == other.baseColor.z
            && baseColor.w == other.baseColor.w
            && metallic == other.metallic
            && roughness == other.roughness
            && emissive == other.emissive
            && materialGroupHash == other.materialGroupHash;
    }
};

struct DrawBatchKeyHash {
    size_t operator()(const DrawBatchKey& key) const noexcept {
        size_t seed = 0;
        auto combine = [&](size_t value) {
            seed ^= value + 0x9e3779b9u + (seed << 6) + (seed >> 2);
        };

        combine(std::hash<ModelResource*>()(key.modelResource));
        combine(std::hash<int>()(key.shaderId));
        combine(std::hash<bool>()(key.castShadow));
        combine(std::hash<int>()(static_cast<int>(key.blendState)));
        combine(std::hash<int>()(static_cast<int>(key.depthState)));
        combine(std::hash<int>()(static_cast<int>(key.rasterizerState)));
        combine(std::hash<float>()(key.baseColor.x));
        combine(std::hash<float>()(key.baseColor.y));
        combine(std::hash<float>()(key.baseColor.z));
        combine(std::hash<float>()(key.baseColor.w));
        combine(std::hash<float>()(key.metallic));
        combine(std::hash<float>()(key.roughness));
        combine(std::hash<float>()(key.emissive));
        combine(std::hash<uint64_t>()(key.materialGroupHash));
        return seed;
    }
};

struct InstanceData {
    DirectX::XMFLOAT4X4 worldMatrix;
    DirectX::XMFLOAT4X4 prevWorldMatrix;
};

struct InstanceBatch {
    DrawBatchKey key;
    std::shared_ptr<ModelResource> modelResource;
    std::vector<InstanceData> instances;
};

struct RenderPacket {
    std::shared_ptr<ModelResource> modelResource;
    DirectX::XMFLOAT4X4 worldMatrix;
    DirectX::XMFLOAT4X4 prevWorldMatrix;

    int shaderId = 1;
    float distanceToCamera = 0.0f;
    bool castShadow = true;

    BlendState      blendState = BlendState::Opaque;
    DepthState      depthState = DepthState::TestAndWrite;
    RasterizerState rasterizerState = RasterizerState::SolidCullBack;

    DirectX::XMFLOAT4 baseColor = { 1.0f, 1.0f, 1.0f, 1.0f };
    float metallic = 0.0f;
    float roughness = 1.0f;
    float emissive = 0.0f;
    std::shared_ptr<MaterialAsset> materialAsset;
    uint64_t materialGroupHash = 0;
};

struct EffectMeshPacket {
    std::shared_ptr<ModelResource> modelResource;
    DirectX::XMFLOAT4X4 worldMatrix;
    DirectX::XMFLOAT4X4 prevWorldMatrix;
    int shaderId = 1;
    float distanceToCamera = 0.0f;
    BlendState blendState = BlendState::Additive;
    DepthState depthState = DepthState::TestOnly;
    RasterizerState rasterizerState = RasterizerState::SolidCullBack;
    DirectX::XMFLOAT4 baseColor = { 1.0f, 1.0f, 1.0f, 1.0f };
    float metallic = 0.0f;
    float roughness = 1.0f;
    float emissive = 0.0f;
    std::shared_ptr<MaterialAsset> materialAsset;
    uint32_t shaderVariantKey = 0;
    uint64_t sortKey = 0;
    float lifetimeFade = 1.0f;

    // Phase A: Mesh Variant System
    EffectMeshVariantParams meshVariantParams;
    // Base (albedo) texture authored on the MeshRenderer node. When non-null
    // EffectMeshPass binds this to slot 0 instead of the FBX material's own
    // albedoMap, so templates can override the source model's texture.
    std::shared_ptr<ITexture> baseTexture;
    std::shared_ptr<ITexture> maskTexture;
    std::shared_ptr<ITexture> normalMapTexture;
    std::shared_ptr<ITexture> flowMapTexture;
    std::shared_ptr<ITexture> subTexture;
    std::shared_ptr<ITexture> emissionTexture;
};

struct EffectParticlePacket {
    uint32_t runtimeInstanceId = 0;
    EffectParticleDrawMode drawMode = EffectParticleDrawMode::Billboard;
    EffectParticleSortMode sortMode = EffectParticleSortMode::BackToFront;
    EffectSpawnShapeType shapeType = EffectSpawnShapeType::Sphere;
    std::shared_ptr<ModelResource> modelResource;
    std::shared_ptr<ITexture> texture;
    DirectX::XMFLOAT3 origin = { 0.0f, 0.0f, 0.0f };
    DirectX::XMFLOAT3 boundsCenter = { 0.0f, 0.0f, 0.0f };
    DirectX::XMFLOAT3 boundsExtents = { 0.5f, 0.5f, 0.5f };
    DirectX::XMFLOAT4 tint = { 1.0f, 1.0f, 1.0f, 1.0f };
    DirectX::XMFLOAT4 tintEnd = { 1.0f, 1.0f, 1.0f, 0.0f };
    DirectX::XMFLOAT3 acceleration = { 0.0f, -0.55f, 0.0f };
    float drag = 0.0f;
    DirectX::XMFLOAT3 shapeParameters = { 0.35f, 0.35f, 0.35f };
    float spinRate = 6.0f;
    float currentTime = 0.0f;
    float duration = 2.0f;
    uint32_t seed = 1;
    uint32_t maxParticles = 0;
    float spawnRate = 32.0f;
    uint32_t burstCount = 0;
    float particleLifetime = 1.0f;
    float startSize = 0.18f;
    float endSize = 0.04f;
    float speed = 1.0f;
    float lifetimeFade = 1.0f;
    float ribbonWidth = 0.08f;
    float ribbonVelocityStretch = 0.30f;
    float sizeCurveBias = 1.0f;
    float alphaCurveBias = 1.0f;
    uint32_t subUvColumns = 1;
    uint32_t subUvRows = 1;
    float subUvFrameRate = 0.0f;
    float curlNoiseStrength = 0.0f;
    float curlNoiseScale = 0.18f;
    float curlNoiseScrollSpeed = 0.20f;
    float vortexStrength = 0.0f;
    bool softParticleEnabled = false;
    float softParticleScale = 96.0f;
    EffectParticleBlendMode blendMode = EffectParticleBlendMode::PremultipliedAlpha;
    float randomSpeedRange = 0.0f;
    float randomSizeRange = 0.0f;
    float randomLifeRange = 0.0f;
    float windStrength = 0.0f;
    DirectX::XMFLOAT3 windDirection = { 1.0f, 0.0f, 0.0f };
    float windTurbulence = 0.0f;
    // Phase 1C: Size curve
    DirectX::XMFLOAT4 sizeCurveValues = { 0.18f, 0.18f, 0.04f, 0.04f };
    DirectX::XMFLOAT4 sizeCurveTimes  = { 0.0f,  0.33f, 0.66f, 1.0f };
    uint32_t sizeCurveKeyCount = 2;
    // Phase 1C: Color gradient
    DirectX::XMFLOAT4 gradientColor0 = { 1.0f, 1.0f, 1.0f, 1.0f };
    DirectX::XMFLOAT4 gradientColor1 = { 1.0f, 1.0f, 1.0f, 1.0f };
    DirectX::XMFLOAT4 gradientColor2 = { 1.0f, 1.0f, 1.0f, 0.0f };
    DirectX::XMFLOAT4 gradientColor3 = { 1.0f, 1.0f, 1.0f, 0.0f };
    DirectX::XMFLOAT2 gradientMidTimes = { 0.33f, 0.66f };
    uint32_t gradientKeyCount = 2;
    // Phase 2: Attractor/Repeller
    DirectX::XMFLOAT4 attractors[4] = {};
    DirectX::XMFLOAT4 attractorRadii = { 5.0f, 5.0f, 5.0f, 5.0f };
    DirectX::XMFLOAT4 attractorFalloff = { 1.0f, 1.0f, 1.0f, 1.0f };
    uint32_t attractorCount = 0;
    // Phase 2: GPU Collision
    bool collisionEnabled = false;
    DirectX::XMFLOAT4 collisionPlane = { 0.0f, 1.0f, 0.0f, 0.0f };
    DirectX::XMFLOAT4 collisionSpheres[4] = {};
    uint32_t collisionSphereCount = 0;
    float collisionRestitution = 0.5f;
    float collisionFriction = 0.3f;
};

struct TrailVertex {
    DirectX::XMFLOAT3 position;
    DirectX::XMFLOAT4 color;
    DirectX::XMFLOAT2 texcoord;
};

struct TrailPacket {
    std::vector<TrailVertex> vertices;
    std::vector<uint32_t> indices;
};

class RenderQueue {
public:
    std::vector<RenderPacket> opaquePackets;
    std::vector<RenderPacket> transparentPackets;
    std::vector<InstanceBatch> opaqueInstanceBatches;
    std::vector<EffectMeshPacket> effectMeshPackets;
    std::vector<EffectParticlePacket> effectParticlePackets;
    std::vector<TrailPacket> trailPackets;
    RenderQueueMetrics metrics;

    void Clear() {
        opaquePackets.clear();
        transparentPackets.clear();
        opaqueInstanceBatches.clear();
        effectMeshPackets.clear();
        effectParticlePackets.clear();
        trailPackets.clear();
        metrics = {};
    }
};
