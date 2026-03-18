#pragma once

#include <vector>
#include <memory>
#include <DirectXMath.h>
#include "RenderState.h"

class ModelResource;

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
};

class RenderQueue {
public:
    std::vector<RenderPacket> opaquePackets;
    std::vector<RenderPacket> transparentPackets;

    void Clear() {
        opaquePackets.clear();
        transparentPackets.clear();
    }
};
