#pragma once
#include "Registry/Registry.h"
#include "System/Query.h"
#include "Component/MeshComponent.h"
#include "Component/TransformComponent.h"
#include "RenderContext/RenderQueue.h"
#include "Component/MaterialComponent.h"
#include <unordered_map>
#include <vector>

class MeshExtractSystem {
public:
    void Extract(Registry& registry, RenderQueue& queue);

private:
    struct ExtractSource
    {
        MeshComponent* mesh = nullptr;
        const TransformComponent* transform = nullptr;
        MaterialComponent* material = nullptr;
    };

    struct ExtractBucket
    {
        std::vector<RenderPacket> opaquePackets;
        std::vector<RenderPacket> transparentPackets;
    };

    std::vector<ExtractSource> m_sources;
    std::vector<ExtractBucket> m_buckets;
    std::unordered_map<DrawBatchKey, size_t, DrawBatchKeyHash> m_batchLookup;
};
