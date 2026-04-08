#include "MeshExtractSystem.h"
#include "Model/Model.h"
#include "Model/ModelResource.h"
#include "Material/MaterialAsset.h"
#include "System/ResourceManager.h"
#include "System/TaskSystem.h"
#include "Console/Logger.h"
#include "Type/TypeInfo.h"
#include <algorithm>
#include <chrono>
#include <cstring>
#include <unordered_map>

namespace
{
    inline void HashCombine(uint64_t& seed, uint64_t value)
    {
        seed ^= value + 0x9e3779b97f4a7c15ull + (seed << 6) + (seed >> 2);
    }

    inline uint64_t HashFloat(float value)
    {
        uint32_t bits = 0;
        static_assert(sizeof(bits) == sizeof(value));
        std::memcpy(&bits, &value, sizeof(bits));
        return std::hash<uint32_t>()(bits);
    }

    uint64_t BuildMaterialGroupHash(const MaterialAsset& material)
    {
        uint64_t seed = 0;
        HashCombine(seed, std::hash<int>()(material.shaderId));
        HashCombine(seed, std::hash<int>()(material.alphaMode));
        HashCombine(seed, HashFloat(material.baseColor.x));
        HashCombine(seed, HashFloat(material.baseColor.y));
        HashCombine(seed, HashFloat(material.baseColor.z));
        HashCombine(seed, HashFloat(material.baseColor.w));
        HashCombine(seed, HashFloat(material.metallic));
        HashCombine(seed, HashFloat(material.roughness));
        HashCombine(seed, HashFloat(material.emissive));
        HashCombine(seed, std::hash<std::string>()(material.diffuseTexturePath));
        HashCombine(seed, std::hash<std::string>()(material.normalTexturePath));
        HashCombine(seed, std::hash<std::string>()(material.metallicRoughnessTexturePath));
        HashCombine(seed, std::hash<std::string>()(material.emissiveTexturePath));
        return seed;
    }
}

void MeshExtractSystem::Extract(Registry& registry, RenderQueue& queue)
{
    using Clock = std::chrono::high_resolution_clock;
    const auto startTime = Clock::now();

    auto defaultMat = ResourceManager::Instance().GetDefaultMaterial();
    m_sources.clear();

    const Signature querySignature = CreateSignature<MeshComponent, TransformComponent>();
    const ComponentTypeID meshType = TypeManager::GetComponentTypeID<MeshComponent>();
    const ComponentTypeID transformType = TypeManager::GetComponentTypeID<TransformComponent>();
    const ComponentTypeID materialType = TypeManager::GetComponentTypeID<MaterialComponent>();

    uint32_t materialResolveCount = 0;
    for (Archetype* archetype : registry.GetAllArchetypes()) {
        if (!archetype || !SignatureMatches(archetype->GetSignature(), querySignature)) {
            continue;
        }

        const size_t entityCount = archetype->GetEntityCount();
        if (entityCount == 0) {
            continue;
        }

        auto* meshColumn = archetype->GetColumn(meshType);
        auto* transformColumn = archetype->GetColumn(transformType);
        auto* materialColumn = archetype->GetColumn(materialType);
        if (!meshColumn || !transformColumn) {
            continue;
        }

        auto* meshes = static_cast<MeshComponent*>(meshColumn->Get(0));
        auto* transforms = static_cast<TransformComponent*>(transformColumn->Get(0));
        auto* materials = materialColumn ? static_cast<MaterialComponent*>(materialColumn->Get(0)) : nullptr;

        const size_t beginIndex = m_sources.size();
        m_sources.resize(beginIndex + entityCount);
        for (size_t i = 0; i < entityCount; ++i) {
            auto& source = m_sources[beginIndex + i];
            source.mesh = &meshes[i];
            source.transform = &transforms[i];
            source.material = materials ? &materials[i] : nullptr;
            if (source.material && !source.material->materialAsset && !source.material->materialAssetPath.empty()) {
                source.material->materialAsset = ResourceManager::Instance().GetMaterial(source.material->materialAssetPath);
                ++materialResolveCount;
            }
        }
    }

    const size_t expectedVisibleCount = m_sources.size();
    if (queue.opaquePackets.capacity() < expectedVisibleCount) {
        ++queue.metrics.opaquePacketVectorGrowths;
    }
    if (queue.transparentPackets.capacity() < expectedVisibleCount) {
        ++queue.metrics.transparentPacketVectorGrowths;
    }

    m_buckets.resize(expectedVisibleCount);
    for (size_t i = 0; i < expectedVisibleCount; ++i) {
        m_buckets[i].opaquePackets.clear();
        m_buckets[i].transparentPackets.clear();
    }
    TaskSystem::Instance().ParallelFor(
        expectedVisibleCount,
        32,
        [&](size_t sourceIndex) {
            const auto& source = m_sources[sourceIndex];
            if (!source.mesh || !source.transform) {
                return;
            }

            MeshComponent& mesh = *source.mesh;
            if (!mesh.isVisible || !mesh.model) {
                return;
            }

            MaterialAsset* activeMat = defaultMat.get();
            if (source.material && source.material->materialAsset) {
                activeMat = source.material->materialAsset.get();
            }

            RenderPacket packet;
            packet.modelResource = mesh.model->GetModelResource();
            packet.worldMatrix = source.transform->worldMatrix;
            packet.prevWorldMatrix = source.transform->prevWorldMatrix;
            packet.castShadow = mesh.castShadow;
            packet.shaderId = activeMat->shaderId;
            packet.baseColor = activeMat->baseColor;
            packet.metallic = activeMat->metallic;
            packet.roughness = activeMat->roughness;
            packet.emissive = activeMat->emissive;
            packet.materialAsset = source.material && source.material->materialAsset
                ? source.material->materialAsset
                : defaultMat;
            packet.materialGroupHash = BuildMaterialGroupHash(*activeMat);

            auto& bucket = m_buckets[sourceIndex];
            const bool isTransparent = (activeMat->alphaMode == 2);
            if (isTransparent) {
                bucket.transparentPackets.push_back(std::move(packet));
            } else {
                bucket.opaquePackets.push_back(std::move(packet));
            }
        });

    queue.opaquePackets.clear();
    queue.transparentPackets.clear();
    queue.opaquePackets.reserve(expectedVisibleCount);
    queue.transparentPackets.reserve(expectedVisibleCount);

    for (auto& bucket : m_buckets) {
        if (!bucket.opaquePackets.empty()) {
            queue.opaquePackets.insert(
                queue.opaquePackets.end(),
                std::make_move_iterator(bucket.opaquePackets.begin()),
                std::make_move_iterator(bucket.opaquePackets.end()));
        }
        if (!bucket.transparentPackets.empty()) {
            queue.transparentPackets.insert(
                queue.transparentPackets.end(),
                std::make_move_iterator(bucket.transparentPackets.begin()),
                std::make_move_iterator(bucket.transparentPackets.end()));
        }
    }

    m_batchLookup.clear();
    m_batchLookup.reserve(queue.opaquePackets.size());
    if (queue.opaqueInstanceBatches.capacity() < queue.opaquePackets.size()) {
        ++queue.metrics.opaqueBatchVectorGrowths;
    }
    queue.opaqueInstanceBatches.reserve(queue.opaquePackets.size());

    uint32_t nonSkinnedOpaquePacketCount = 0;
    uint32_t skinnedOpaquePacketCount = 0;
    for (const RenderPacket& packet : queue.opaquePackets) {
        const bool isSkinnedPacket = packet.modelResource && packet.modelResource->HasSkinnedMeshes();
        if (isSkinnedPacket) {
            ++skinnedOpaquePacketCount;
        } else {
            ++nonSkinnedOpaquePacketCount;
        }

        DrawBatchKey batchKey{};
        batchKey.modelResource = packet.modelResource.get();
        batchKey.shaderId = packet.shaderId;
        batchKey.castShadow = packet.castShadow;
        batchKey.blendState = packet.blendState;
        batchKey.depthState = packet.depthState;
        batchKey.rasterizerState = packet.rasterizerState;
        batchKey.baseColor = packet.baseColor;
        batchKey.metallic = packet.metallic;
        batchKey.roughness = packet.roughness;
        batchKey.emissive = packet.emissive;
        batchKey.materialAsset = packet.materialAsset.get();
        batchKey.materialGroupHash = packet.materialGroupHash;

        auto [it, inserted] = m_batchLookup.emplace(batchKey, queue.opaqueInstanceBatches.size());
        if (inserted) {
            InstanceBatch batch{};
            batch.key = batchKey;
            batch.modelResource = packet.modelResource;
            queue.opaqueInstanceBatches.push_back(std::move(batch));
        }

        InstanceData instance{};
        instance.worldMatrix = packet.worldMatrix;
        instance.prevWorldMatrix = packet.prevWorldMatrix;
        queue.opaqueInstanceBatches[it->second].instances.push_back(instance);
    }

    const auto sortStart = Clock::now();
    std::sort(
        queue.opaqueInstanceBatches.begin(),
        queue.opaqueInstanceBatches.end(),
        [](const InstanceBatch& lhs, const InstanceBatch& rhs) {
            if (lhs.key.shaderId != rhs.key.shaderId) {
                return lhs.key.shaderId < rhs.key.shaderId;
            }
            if (lhs.key.materialGroupHash != rhs.key.materialGroupHash) {
                return lhs.key.materialGroupHash < rhs.key.materialGroupHash;
            }
            return lhs.modelResource.get() < rhs.modelResource.get();
        });
    const auto sortEnd = Clock::now();

    uint32_t maxInstancesPerBatch = 0;
    for (const InstanceBatch& batch : queue.opaqueInstanceBatches) {
        const uint32_t count = static_cast<uint32_t>(batch.instances.size());
        if (count > maxInstancesPerBatch) {
            maxInstancesPerBatch = count;
        }
    }

    queue.metrics.meshExtractMs =
        std::chrono::duration<double, std::milli>(Clock::now() - startTime).count();
    queue.metrics.materialResolveCount = materialResolveCount;
    queue.metrics.materialGroupCount = static_cast<uint32_t>(m_batchLookup.size());
    queue.metrics.opaquePacketCount = static_cast<uint32_t>(queue.opaquePackets.size());
    queue.metrics.transparentPacketCount = static_cast<uint32_t>(queue.transparentPackets.size());
    queue.metrics.opaqueBatchCount = static_cast<uint32_t>(queue.opaqueInstanceBatches.size());
    queue.metrics.maxInstancesPerBatch = maxInstancesPerBatch;
    queue.metrics.nonSkinnedOpaquePacketCount = nonSkinnedOpaquePacketCount;
    queue.metrics.skinnedOpaquePacketCount = skinnedOpaquePacketCount;
    queue.metrics.batchSortMs =
        std::chrono::duration<double, std::milli>(sortEnd - sortStart).count();

    static size_t s_lastOpaqueCount = static_cast<size_t>(-1);
    static size_t s_lastTransparentCount = static_cast<size_t>(-1);
    static size_t s_lastBatchCount = static_cast<size_t>(-1);
    if (s_lastOpaqueCount != queue.opaquePackets.size() ||
        s_lastTransparentCount != queue.transparentPackets.size() ||
        s_lastBatchCount != queue.opaqueInstanceBatches.size()) {
        LOG_INFO("[MeshExtract] opaque=%zu transparent=%zu batches=%zu",
            queue.opaquePackets.size(), queue.transparentPackets.size(), queue.opaqueInstanceBatches.size());
        s_lastOpaqueCount = queue.opaquePackets.size();
        s_lastTransparentCount = queue.transparentPackets.size();
        s_lastBatchCount = queue.opaqueInstanceBatches.size();
    }
}
