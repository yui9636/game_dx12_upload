#include "TimelineHitboxSystem.h"
#include "TimelineComponent.h"
#include "TimelineItemBuffer.h"
#include "Component/ColliderComponent.h"
#include "Component/TransformComponent.h"
#include "Collision/CollisionManager.h"
#include "Registry/Registry.h"
#include "Component/ComponentSignature.h"
#include "Type/TypeInfo.h"
#include "Archetype/Archetype.h"

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

            // Entity world position (for sphere center offset)
            DirectX::XMFLOAT3 worldPos = { 0, 0, 0 };
            if (txCol) {
                auto& tx = *static_cast<TransformComponent*>(txCol->Get(i));
                worldPos = tx.worldPosition;
            }

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
                    DirectX::XMFLOAT3 center = {
                        worldPos.x + item.hb.offsetLocal.x,
                        worldPos.y + item.hb.offsetLocal.y,
                        worldPos.z + item.hb.offsetLocal.z
                    };
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
