#include "TimelineVFXSystem.h"
#include "TimelineComponent.h"
#include "TimelineItemBuffer.h"
#include "Component/MeshComponent.h"
#include "Component/TransformComponent.h"
#include "EffectRuntime/EffectService.h"
#include "Model/Model.h"
#include "Registry/Registry.h"
#include "Component/ComponentSignature.h"
#include "Type/TypeInfo.h"
#include "Archetype/Archetype.h"
#include "Transform/NodeAttachmentUtils.h"
#include <DirectXMath.h>

namespace
{
    DirectX::XMFLOAT4 QuaternionFromEulerDeg(const DirectX::XMFLOAT3& eulerDeg)
    {
        DirectX::XMVECTOR rotation = DirectX::XMQuaternionRotationRollPitchYaw(
            DirectX::XMConvertToRadians(eulerDeg.x),
            DirectX::XMConvertToRadians(eulerDeg.y),
            DirectX::XMConvertToRadians(eulerDeg.z));
        DirectX::XMFLOAT4 result;
        DirectX::XMStoreFloat4(&result, rotation);
        return result;
    }

    DirectX::XMFLOAT3 ResolveTimelineNodeWorldPosition(
        Registry& registry,
        EntityID entity,
        const TransformComponent* transform,
        int nodeIndex,
        const DirectX::XMFLOAT3& offsetLocal)
    {
        if (!transform) {
            return offsetLocal;
        }

        auto* mesh = registry.GetComponent<MeshComponent>(entity);
        if (!mesh || !mesh->model || nodeIndex < 0 || nodeIndex >= static_cast<int>(mesh->model->GetNodes().size())) {
            return {
                transform->worldPosition.x + offsetLocal.x,
                transform->worldPosition.y + offsetLocal.y,
                transform->worldPosition.z + offsetLocal.z
            };
        }

        DirectX::XMFLOAT4X4 world;
        int cacheIndex = nodeIndex;
        if (NodeAttachmentUtils::TryGetBoneWorldMatrix(
            mesh->model.get(),
            transform->worldMatrix,
            mesh->model->GetNodes()[nodeIndex].name,
            cacheIndex,
            offsetLocal,
            { 0.0f, 0.0f, 0.0f },
            { 1.0f, 1.0f, 1.0f },
            NodeAttachmentSpace::NodeLocal,
            world)) {
            return { world._41, world._42, world._43 };
        }

        return {
            transform->worldPosition.x + offsetLocal.x,
            transform->worldPosition.y + offsetLocal.y,
            transform->worldPosition.z + offsetLocal.z
        };
    }
}

void TimelineVFXSystem::Update(Registry& registry) {
    Signature sig = CreateSignature<TimelineComponent, TimelineItemBuffer>();
    for (auto* arch : registry.GetAllArchetypes()) {
        if (!SignatureMatches(arch->GetSignature(), sig)) continue;
        auto* tlCol  = arch->GetColumn(TypeManager::GetComponentTypeID<TimelineComponent>());
        auto* bufCol = arch->GetColumn(TypeManager::GetComponentTypeID<TimelineItemBuffer>());
        auto* txCol  = arch->GetColumn(TypeManager::GetComponentTypeID<TransformComponent>());
        if (!tlCol || !bufCol) continue;
        const auto& entities = arch->GetEntities();

        for (size_t i = 0; i < arch->GetEntityCount(); ++i) {
            auto& tl  = *static_cast<TimelineComponent*>(tlCol->Get(i));
            auto& buf = *static_cast<TimelineItemBuffer*>(bufCol->Get(i));
            const EntityID ownerEntity = entities[i];

            TransformComponent* tx = txCol ? static_cast<TransformComponent*>(txCol->Get(i)) : nullptr;

            for (auto& item : buf.items) {
                if (item.type != 2) continue;
                bool inside = tl.currentFrame >= item.start && tl.currentFrame <= item.end;
                const DirectX::XMFLOAT3 spawnPos = ResolveTimelineNodeWorldPosition(
                    registry,
                    ownerEntity,
                    tx,
                    item.vfx.nodeIndex,
                    item.vfx.offsetLocal);
                const DirectX::XMFLOAT4 spawnRot = QuaternionFromEulerDeg(item.vfx.offsetRotDeg);

                if (inside && !item.vfxActive) {
                    EffectPlayDesc desc;
                    desc.assetPath = item.vfx.assetId;
                    desc.position = spawnPos;
                    desc.rotation = spawnRot;
                    desc.scale = item.vfx.offsetScale;
                    desc.loop = !item.vfx.fireOnEnterOnly;
                    desc.debugName = "Timeline Effect";
                    item.vfxHandle = EffectService::Instance().PlayWorld(registry, desc);
                    if (item.vfxHandle.IsValid()) {
                        const float duration = (tl.fps > 0.0f)
                            ? static_cast<float>(item.end - item.start) / tl.fps
                            : 2.0f;
                        const float effectAge = (tl.fps > 0.0f)
                            ? static_cast<float>(tl.currentFrame - item.start) / tl.fps
                            : 0.0f;
                        EffectService::Instance().Seek(registry, item.vfxHandle, effectAge, duration, !item.vfx.fireOnEnterOnly);
                    }
                    item.vfxActive = true;
                }
                else if (inside && item.vfxActive && item.vfxHandle.IsValid()) {
                    float effectAge = (tl.fps > 0.0f)
                        ? static_cast<float>(tl.currentFrame - item.start) / tl.fps
                        : 0.0f;
                    const float duration = (tl.fps > 0.0f)
                        ? static_cast<float>(item.end - item.start) / tl.fps
                        : 2.0f;
                    EffectService::Instance().Seek(registry, item.vfxHandle, effectAge, duration, !item.vfx.fireOnEnterOnly);
                    EffectService::Instance().SetWorldTransform(registry, item.vfxHandle, spawnPos, spawnRot, item.vfx.offsetScale);
                }
                else if (!inside && item.vfxActive) {
                    EffectService::Instance().Stop(registry, item.vfxHandle, true);
                    item.vfxHandle.Reset();
                    item.vfxActive = false;
                }
            }
        }
    }
}
