#include "HitboxTrackingSystem.h"
#include "HitboxTrackingComponent.h"
#include "TimelineComponent.h"
#include "TimelineItemBuffer.h"
#include "Registry/Registry.h"
#include "Component/ComponentSignature.h"
#include "Type/TypeInfo.h"
#include "Archetype/Archetype.h"

void HitboxTrackingSystem::Update(Registry& registry) {
    Signature sig = CreateSignature<HitboxTrackingComponent, TimelineComponent, TimelineItemBuffer>();
    for (auto* arch : registry.GetAllArchetypes()) {
        if (!SignatureMatches(arch->GetSignature(), sig)) continue;
        auto* hitCol = arch->GetColumn(TypeManager::GetComponentTypeID<HitboxTrackingComponent>());
        auto* tlCol = arch->GetColumn(TypeManager::GetComponentTypeID<TimelineComponent>());
        auto* bufCol = arch->GetColumn(TypeManager::GetComponentTypeID<TimelineItemBuffer>());
        if (!hitCol || !tlCol || !bufCol) continue;

        for (size_t i = 0; i < arch->GetEntityCount(); ++i) {
            auto& hit = *static_cast<HitboxTrackingComponent*>(hitCol->Get(i));
            auto& tl = *static_cast<TimelineComponent*>(tlCol->Get(i));
            auto& buf = *static_cast<TimelineItemBuffer*>(bufCol->Get(i));

            bool inHitbox = false;
            for (auto& item : buf.items) {
                if (item.type == 0 && tl.currentFrame >= item.start && tl.currentFrame <= item.end) {
                    inHitbox = true;
                    if (hit.lastHitboxStart != item.start) {
                        hit.hitEntityCount = 0;
                        hit.lastHitboxStart = item.start;
                    }
                    break;
                }
            }
            if (!inHitbox) {
                hit.lastHitboxStart = -1;
            }
        }
    }
}
