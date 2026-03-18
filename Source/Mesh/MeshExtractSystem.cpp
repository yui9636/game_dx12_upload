#include "MeshExtractSystem.h"
#include "Model/Model.h"
#include "Material/MaterialAsset.h"
#include "System/ResourceManager.h"
#include "Console/Logger.h"

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
        if (matComp && matComp->materialAsset) {
            activeMat = matComp->materialAsset.get();
        }

        packet.shaderId = activeMat->shaderId;
        packet.baseColor = activeMat->baseColor;
        packet.metallic = activeMat->metallic;
        packet.roughness = activeMat->roughness;
        packet.emissive = activeMat->emissive;

        const bool isTransparent = (activeMat->alphaMode == 2);
        if (isTransparent) {
            queue.transparentPackets.push_back(packet);
        }
        else {
            queue.opaquePackets.push_back(packet);
        }
    });

    static size_t s_lastOpaqueCount = static_cast<size_t>(-1);
    static size_t s_lastTransparentCount = static_cast<size_t>(-1);
    if (s_lastOpaqueCount != queue.opaquePackets.size() || s_lastTransparentCount != queue.transparentPackets.size()) {
        LOG_INFO("[MeshExtract] opaque=%zu transparent=%zu", queue.opaquePackets.size(), queue.transparentPackets.size());
        s_lastOpaqueCount = queue.opaquePackets.size();
        s_lastTransparentCount = queue.transparentPackets.size();
    }
}
