#include "TimelineHitboxSystem.h"
#include "TimelineComponent.h"
#include "TimelineItemBuffer.h"
#include "Component/ColliderComponent.h"
#include "Component/MeshComponent.h"
#include "Component/TransformComponent.h"
#include "Collision/CollisionManager.h"
#include "Model/Model.h"
#include "Registry/Registry.h"
#include "Component/ComponentSignature.h"
#include "Type/TypeInfo.h"
#include "Archetype/Archetype.h"
#include "Transform/NodeAttachmentUtils.h"

namespace
{
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
        if (!mesh || !mesh->model || nodeIndex < 0) {
            return {
                transform->worldPosition.x + offsetLocal.x,
                transform->worldPosition.y + offsetLocal.y,
                transform->worldPosition.z + offsetLocal.z
            };
        }

        const auto& nodes = mesh->model->GetNodes();
        if (nodeIndex >= static_cast<int>(nodes.size())) {
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
            nodes[nodeIndex].name,
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

void TimelineHitboxSystem::Update(Registry& registry) {
    Signature sig = CreateSignature<TimelineComponent, TimelineItemBuffer, ColliderComponent>();
    for (auto* arch : registry.GetAllArchetypes()) {
        if (!SignatureMatches(arch->GetSignature(), sig)) continue;
        auto* tlCol  = arch->GetColumn(TypeManager::GetComponentTypeID<TimelineComponent>());
        auto* bufCol = arch->GetColumn(TypeManager::GetComponentTypeID<TimelineItemBuffer>());
        auto* colCol = arch->GetColumn(TypeManager::GetComponentTypeID<ColliderComponent>());
        auto* txCol  = arch->GetColumn(TypeManager::GetComponentTypeID<TransformComponent>());
        if (!tlCol || !bufCol || !colCol) continue;

        auto& cm = CollisionManager::Instance();

        for (size_t i = 0; i < arch->GetEntityCount(); ++i) {
            auto& tl  = *static_cast<TimelineComponent*>(tlCol->Get(i));
            auto& buf = *static_cast<TimelineItemBuffer*>(bufCol->Get(i));
            auto& col = *static_cast<ColliderComponent*>(colCol->Get(i));
            const EntityID ownerEntity = arch->GetEntities()[i];

            // Entity world position (for sphere center offset)
            TransformComponent* tx = txCol ? static_cast<TransformComponent*>(txCol->Get(i)) : nullptr;

            // Collect active hitbox item indices
            for (int idx = 0; idx < static_cast<int>(buf.items.size()); ++idx) {
                auto& item = buf.items[idx];
                if (item.type != 0) continue;

                bool active = tl.currentFrame >= item.start && tl.currentFrame <= item.end;
                int tag = idx + 1; // runtimeTag: 1-based item index

                // Find existing collider element by tag
                ColliderComponent::Element* found = nullptr;
                for (auto& elem : col.elements) {
                    if (elem.runtimeTag == tag) { found = &elem; break; }
                }

                if (active) {
                    DirectX::XMFLOAT3 center = ResolveTimelineNodeWorldPosition(
                        registry,
                        ownerEntity,
                        tx,
                        item.hb.nodeIndex,
                        item.hb.offsetLocal);
                    if (found) {
                        // Update existing
                        found->radius = item.hb.radius;
                        found->offsetLocal = item.hb.offsetLocal;
                        found->enabled = true;
                        if (found->registeredId) {
                            cm.UpdateSphere(found->registeredId, { center, item.hb.radius });
                            cm.SetEnabled(found->registeredId, true);
                        }
                    } else {
                        // Create new attack sphere
                        ColliderComponent::Element elem;
                        elem.type = ColliderShape::Sphere;
                        elem.attribute = ColliderAttribute::Attack;
                        elem.radius = item.hb.radius;
                        elem.offsetLocal = item.hb.offsetLocal;
                        elem.nodeIndex = item.hb.nodeIndex;
                        elem.runtimeTag = tag;
                        elem.enabled = true;
                        elem.registeredId = cm.AddSphere({ center, item.hb.radius }, nullptr, ColliderAttribute::Attack);
                        col.elements.push_back(elem);
                    }
                } else if (found) {
                    // Deactivate
                    if (found->registeredId) {
                        cm.SetEnabled(found->registeredId, false);
                    }
                    found->enabled = false;
                }
            }

            // Cleanup: remove runtime elements whose items no longer exist
            for (auto it = col.elements.begin(); it != col.elements.end(); ) {
                if (it->runtimeTag > 0 && it->runtimeTag > static_cast<int>(buf.items.size())) {
                    if (it->registeredId) cm.Remove(it->registeredId);
                    it = col.elements.erase(it);
                } else {
                    ++it;
                }
            }
        }
    }
}
