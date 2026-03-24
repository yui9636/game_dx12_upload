#pragma once

#include <vector>
#include <memory>
#include <DirectXMath.h>
#include "RenderState.h"

class ModelResource;
class MaterialAsset;

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

    void Clear() {
        opaquePackets.clear();
        transparentPackets.clear();
        opaqueInstanceBatches.clear();
    }
};
