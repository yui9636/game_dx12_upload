#pragma once

#include <vector>
#include <memory>
#include <cstdint>
#include <cstddef>
#include <functional>
#include <string>
#include <DirectXMath.h>
#include "Entity/Entity.h"
#include "RenderState.h"
#include "EffectRuntime/EffectGraphAsset.h"

class ModelResource;
class MaterialAsset;
class IBuffer;
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

    // Phase A は Mesh Variant System。
    EffectMeshVariantParams meshVariantParams;
    // MeshRenderer ノードで指定された base/albedo テクスチャ。非 null の場合、
    // EffectMeshPass は FBX material 側の albedoMap ではなくこれを slot 0 へバインドする。
    // テンプレートから元モデルのテクスチャを差し替えられるようにする。
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
    // Phase 1C: サイズカーブ。
    DirectX::XMFLOAT4 sizeCurveValues = { 0.18f, 0.18f, 0.04f, 0.04f };
    DirectX::XMFLOAT4 sizeCurveTimes  = { 0.0f,  0.33f, 0.66f, 1.0f };
    uint32_t sizeCurveKeyCount = 2;
    // Phase 1C: カラーグラデーション。
    DirectX::XMFLOAT4 gradientColor0 = { 1.0f, 1.0f, 1.0f, 1.0f };
    DirectX::XMFLOAT4 gradientColor1 = { 1.0f, 1.0f, 1.0f, 1.0f };
    DirectX::XMFLOAT4 gradientColor2 = { 1.0f, 1.0f, 1.0f, 0.0f };
    DirectX::XMFLOAT4 gradientColor3 = { 1.0f, 1.0f, 1.0f, 0.0f };
    DirectX::XMFLOAT2 gradientMidTimes = { 0.33f, 0.66f };
    uint32_t gradientKeyCount = 2;
    // Phase 2 は Attractor / Repeller。
    DirectX::XMFLOAT4 attractors[4] = {};
    DirectX::XMFLOAT4 attractorRadii = { 5.0f, 5.0f, 5.0f, 5.0f };
    DirectX::XMFLOAT4 attractorFalloff = { 1.0f, 1.0f, 1.0f, 1.0f };
    uint32_t attractorCount = 0;
    // Phase 2 は GPU Collision。
    bool collisionEnabled = false;
    DirectX::XMFLOAT4 collisionPlane = { 0.0f, 1.0f, 0.0f, 0.0f };
    DirectX::XMFLOAT4 collisionSpheres[4] = {};
    uint32_t collisionSphereCount = 0;
    float collisionRestitution = 0.5f;
    float collisionFriction = 0.3f;
    // MeshParticle Phase 2: Mesh 描画モード用 (drawMode == Mesh 時のみ有効)
    DirectX::XMFLOAT3 meshInitialScale = { 1.0f, 1.0f, 1.0f };
    float             meshScaleRandom = 0.0f;
    DirectX::XMFLOAT3 meshAngularAxis = { 0.0f, 1.0f, 0.0f };
    float             meshAngularSpeed = 0.0f;
    DirectX::XMFLOAT3 meshAngularOrientRandom = { 0.0f, 0.0f, 0.0f };
    float             meshAngularSpeedRandom = 0.0f;
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

struct UI2DLayoutNode {
    EntityID entity = Entity::NULL_ID;
    EntityID parent = Entity::NULL_ID;
    bool screenSpaceOverlay = true;
    bool pixelSnap = false;
    DirectX::XMFLOAT3 worldPosition = { 0.0f, 0.0f, 0.0f };
    DirectX::XMFLOAT4 worldRotation = { 0.0f, 0.0f, 0.0f, 1.0f };
    DirectX::XMFLOAT3 worldScale = { 1.0f, 1.0f, 1.0f };
    DirectX::XMFLOAT2 anchoredPosition = { 0.0f, 0.0f };
    DirectX::XMFLOAT2 sizeDelta = { 100.0f, 100.0f };
    DirectX::XMFLOAT2 anchorMin = { 0.5f, 0.5f };
    DirectX::XMFLOAT2 anchorMax = { 0.5f, 0.5f };
    DirectX::XMFLOAT2 pivot = { 0.5f, 0.5f };
    DirectX::XMFLOAT2 scale2D = { 1.0f, 1.0f };
    float rotationZ = 0.0f;
};

struct UI2DSpritePacket {
    EntityID entity = Entity::NULL_ID;
    std::string textureAssetPath;
    DirectX::XMFLOAT4 tint = { 1.0f, 1.0f, 1.0f, 1.0f };
    DirectX::XMFLOAT3 worldPosition = { 0.0f, 0.0f, 0.0f };
    DirectX::XMFLOAT4 worldRotation = { 0.0f, 0.0f, 0.0f, 1.0f };
    DirectX::XMFLOAT3 worldScale = { 1.0f, 1.0f, 1.0f };
    DirectX::XMFLOAT2 sizeDelta = { 100.0f, 100.0f };
    DirectX::XMFLOAT2 pivot = { 0.5f, 0.5f };
    bool screenSpaceOverlay = true;
    bool pixelSnap = false;
    bool fillClipEnabled = false;
    float fillRatio = 1.0f;
    int fillDirection = 0;
};

struct UI2DTextPacket {
    EntityID entity = Entity::NULL_ID;
    std::string text;
    std::string fontAssetPath;
    float fontSize = 32.0f;
    DirectX::XMFLOAT4 color = { 1.0f, 1.0f, 1.0f, 1.0f };
    int alignment = 1;
    DirectX::XMFLOAT3 worldPosition = { 0.0f, 0.0f, 0.0f };
    DirectX::XMFLOAT4 worldRotation = { 0.0f, 0.0f, 0.0f, 1.0f };
    DirectX::XMFLOAT3 worldScale = { 1.0f, 1.0f, 1.0f };
    DirectX::XMFLOAT2 sizeDelta = { 100.0f, 32.0f };
    DirectX::XMFLOAT2 pivot = { 0.5f, 0.5f };
    bool screenSpaceOverlay = true;
    bool pixelSnap = false;
};

// Terrain チャンク 1 枚分の描画リクエスト。TerrainRenderPass が消費する。
struct TerrainChunkDrawCall {
    IBuffer* vertexBuffer     = nullptr;
    IBuffer* indexBuffer      = nullptr;
    uint32_t indexCount       = 0;
    DirectX::XMFLOAT3 chunkWorldOffset = { 0.0f, 0.0f, 0.0f };
    DirectX::XMFLOAT3 boundsCenter = { 0.0f, 0.0f, 0.0f };
    DirectX::XMFLOAT3 boundsExtents = { 0.0f, 0.0f, 0.0f };
    float worldSizeX  = 1280.0f;
    float worldSizeZ  = 1280.0f;
    float heightScale = 64.0f;
    // スプラットマップと 3 レイヤーの PBR テクスチャ (null 許容)
    ITexture* splatTexture        = nullptr;
    ITexture* albedoTextures[3]   = {};
    ITexture* normalTextures[3]   = {};   // タンジェントスペース法線マップ (RGB)
    ITexture* mraTextures[3]      = {};   // R=Metallic, G=Roughness, B=AO
    float     layerTileScales[3]  = { 8.0f, 6.0f, 4.0f };
};

// 草インスタンス描画リクエスト (Terrain エンティティごとに 1 つ)。
struct GrassDrawCall {
    IBuffer*  meshVertexBuffer = nullptr;
    IBuffer*  meshIndexBuffer  = nullptr;
    uint32_t  meshIndexCount   = 0;
    uint32_t  meshVertexStride = 0;        // bytes per model vertex
    ITexture* albedoTexture    = nullptr;  // grass blade albedo (with alpha)
    float     alphaCutoff      = 0.5f;
    IBuffer*  instanceBuffer   = nullptr;
    uint32_t  instanceCount    = 0;
    DirectX::XMFLOAT3 boundsCenter  = { 0.0f, 0.0f, 0.0f };
    DirectX::XMFLOAT3 boundsExtents = { 0.0f, 0.0f, 0.0f };
    DirectX::XMFLOAT3 meshLocalMin  = { 0.0f, 0.0f, 0.0f };
    DirectX::XMFLOAT3 meshLocalMax  = { 1.0f, 1.0f, 1.0f };
    DirectX::XMFLOAT3 windDirection = { 1.0f, 0.0f, 0.0f };
    float windStrength = 0.35f;
    float windSpeed    = 1.4f;
    DirectX::XMFLOAT3 colorBottom = { 0.16f, 0.28f, 0.10f };
    DirectX::XMFLOAT3 colorTop    = { 0.55f, 0.78f, 0.30f };
    float drawDistance = 250.0f;
    bool  useWind     = true;   // false for rigid props (rocks etc)
};

// Terrain に紐づく水面メッシュの描画リクエスト。
struct TerrainWaterDrawCall {
    IBuffer* vertexBuffer = nullptr;
    IBuffer* indexBuffer = nullptr;
    uint32_t indexCount = 0;
    DirectX::XMFLOAT3 worldOffset = { 0.0f, 0.0f, 0.0f };
    float worldSizeX = 1280.0f;
    float worldSizeZ = 1280.0f;
    float seaLevel = 4.0f;
    DirectX::XMFLOAT4 shallowColor = { 0.12f, 0.44f, 0.56f, 0.55f };
    DirectX::XMFLOAT4 deepColor = { 0.015f, 0.11f, 0.24f, 0.68f };
    float depthFade = 5.0f;
    float waveSpeed = 0.45f;
    float waveScale = 0.035f;
};

class RenderQueue {
public:
    std::vector<RenderPacket> opaquePackets;
    std::vector<RenderPacket> transparentPackets;
    std::vector<InstanceBatch> opaqueInstanceBatches;
    std::vector<EffectMeshPacket> effectMeshPackets;
    std::vector<EffectParticlePacket> effectParticlePackets;
    std::vector<TrailPacket> trailPackets;
    std::vector<UI2DLayoutNode> ui2DLayoutNodes;
    std::vector<UI2DSpritePacket> ui2DSpritePackets;
    std::vector<UI2DTextPacket> ui2DTextPackets;
    std::vector<TerrainChunkDrawCall> terrainChunks;
    std::vector<TerrainWaterDrawCall> terrainWater;
    std::vector<GrassDrawCall>        grassDraws;
    RenderQueueMetrics metrics;

    void Clear() {
        opaquePackets.clear();
        transparentPackets.clear();
        opaqueInstanceBatches.clear();
        effectMeshPackets.clear();
        effectParticlePackets.clear();
        trailPackets.clear();
        ui2DLayoutNodes.clear();
        ui2DSpritePackets.clear();
        ui2DTextPackets.clear();
        terrainChunks.clear();
        terrainWater.clear();
        grassDraws.clear();
        metrics = {};
    }
};
