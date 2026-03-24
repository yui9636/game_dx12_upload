#pragma once

#include <vector>
#include <memory>
#include <cstdint>
#include <cstddef>
#include <functional>
#include <DirectXMath.h>
#include "RenderState.h"

class ModelResource;
class MaterialAsset;

struct RenderQueueMetrics {
    double meshExtractMs = 0.0;
    uint32_t materialResolveCount = 0;
    uint32_t opaquePacketCount = 0;
    uint32_t transparentPacketCount = 0;
    uint32_t opaqueBatchCount = 0;
    uint32_t maxInstancesPerBatch = 0;
    uint32_t opaquePacketVectorGrowths = 0;
    uint32_t transparentPacketVectorGrowths = 0;
    uint32_t opaqueBatchVectorGrowths = 0;
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
    std::shared_ptr<MaterialAsset> materialAsset;

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
            && materialAsset.get() == other.materialAsset.get();
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
        combine(std::hash<MaterialAsset*>()(key.materialAsset.get()));
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
};

class RenderQueue {
public:
    std::vector<RenderPacket> opaquePackets;
    std::vector<RenderPacket> transparentPackets;
    std::vector<InstanceBatch> opaqueInstanceBatches;
    RenderQueueMetrics metrics;

    void Clear() {
        opaquePackets.clear();
        transparentPackets.clear();
        opaqueInstanceBatches.clear();
        metrics = {};
    }
};
