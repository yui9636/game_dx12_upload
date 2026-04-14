#include "TrailSystem.h"
#include "Component/TrailComponent.h"
#include "Component/TransformComponent.h"
#include "Component/ComponentSignature.h"
#include "Archetype/Archetype.h"
#include "Registry/Registry.h"
#include "Type/TypeInfo.h"
#include <cmath>

using namespace DirectX;

void TrailSystem::Update(Registry& registry, float deltaTime)
{
    for (auto* archetype : registry.GetAllArchetypes()) {
        if (!archetype ||
            !SignatureMatches(archetype->GetSignature(), CreateSignature<TrailComponent, TransformComponent>())) {
            continue;
        }

        auto* trailCol = archetype->GetColumn(TypeManager::GetComponentTypeID<TrailComponent>());
        auto* transCol = archetype->GetColumn(TypeManager::GetComponentTypeID<TransformComponent>());
        if (!trailCol || !transCol) continue;

        auto* trails = static_cast<TrailComponent*>(trailCol->Get(0));
        auto* transforms = static_cast<TransformComponent*>(transCol->Get(0));

        for (size_t row = 0; row < archetype->GetEntityCount(); ++row)
        {
            auto& trail = trails[row];
            if (!trail.enabled) continue;

            const auto& transform = transforms[row];
            trail.totalTime += deltaTime;

            const XMFLOAT3& pos = transform.worldPosition;
            bool shouldAdd = trail.points.empty();
            if (!shouldAdd)
            {
                const auto& last = trail.points.back();
                float dx = pos.x - last.position.x;
                float dy = pos.y - last.position.y;
                float dz = pos.z - last.position.z;
                shouldAdd = (dx * dx + dy * dy + dz * dz) >= trail.minDistance * trail.minDistance;
            }

            if (shouldAdd)
            {
                TrailComponent::TrailPoint pt;
                pt.position = pos;
                pt.up = { 0.0f, 1.0f, 0.0f };
                pt.timeStamp = trail.totalTime;
                trail.points.push_back(pt);
            }

            // Remove expired points
            const float cutoff = trail.totalTime - trail.lifetime;
            while (!trail.points.empty() && trail.points.front().timeStamp < cutoff) {
                trail.points.erase(trail.points.begin());
            }

            // Cap max points
            while (static_cast<int>(trail.points.size()) > trail.maxPoints) {
                trail.points.erase(trail.points.begin());
            }
        }
    }
}
