#include "MeshExtractSystem.h"
#include "Model/Model.h"
#include "Material/MaterialAsset.h"
#include "System/ResourceManager.h"
#include "Console/Logger.h"
#include <algorithm>

void MeshExtractSystem::Extract(Registry& registry, RenderQueue& queue)
{
    Query<MeshComponent, TransformComponent> query(registry);

    auto defaultMat = ResourceManager::Instance().GetDefaultMaterial();

    query.ForEachWithEntity([&](EntityID entity, MeshComponent& mesh, const TransformComponent& transform) {
        if (!mesh.isVisible || !mesh.model) return;

        RenderPacket packet;
        packet.modelResource = mesh.model->GetModelResource();
        packet.worldMatrix = transform.worldMatrix;
        packet.prevWorldMatrix = transform.prevWorldMatrix;
        packet.castShadow = mesh.castShadow;

        MaterialAsset* activeMat = defaultMat.get();
        auto* matComp = registry.GetComponent<MaterialComponent>(entity);
        if (matComp) {
            if (!matComp->materialAsset && !matComp->materialAssetPath.empty()) {
                matComp->materialAsset = ResourceManager::Instance().GetMaterial(matComp->materialAssetPath);
            }
            if (matComp->materialAsset) {
                activeMat = matComp->materialAsset.get();
            }
        }

        packet.shaderId = activeMat->shaderId;
        packet.baseColor = activeMat->baseColor;
        packet.metallic = activeMat->metallic;
        packet.roughness = activeMat->roughness;
        packet.emissive = activeMat->emissive;
        packet.materialAsset = matComp ? matComp->materialAsset : defaultMat;

        const bool isTransparent = (activeMat->alphaMode == 2);
        if (isTransparent) {
            queue.transparentPackets.push_back(packet);
        }
        else {
            queue.opaquePackets.push_back(packet);

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
            batchKey.materialAsset = packet.materialAsset;

            auto it = std::find_if(
                queue.opaqueInstanceBatches.begin(),
                queue.opaqueInstanceBatches.end(),
                [&](const InstanceBatch& batch) { return batch.key == batchKey; });

            if (it == queue.opaqueInstanceBatches.end()) {
                InstanceBatch batch{};
                batch.key = batchKey;
                batch.modelResource = packet.modelResource;
                queue.opaqueInstanceBatches.push_back(std::move(batch));
                it = std::prev(queue.opaqueInstanceBatches.end());
            }

            InstanceData instance{};
            instance.worldMatrix = packet.worldMatrix;
            instance.prevWorldMatrix = packet.prevWorldMatrix;
            it->instances.push_back(instance);
        }
    });

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
