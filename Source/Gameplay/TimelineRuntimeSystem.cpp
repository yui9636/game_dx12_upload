#include "TimelineRuntimeSystem.h"
#include "Gameplay/TimelineComponent.h"
#include "Gameplay/PlaybackComponent.h"
#include "Registry/Registry.h"
#include "Component/ComponentSignature.h"

void TimelineRuntimeSystem::Update(Registry& registry, float dt)
{
    Signature sig = CreateSignature<TimelineComponent, PlaybackComponent>();

    for (auto* arch : registry.GetAllArchetypes()) {
        if (!SignatureMatches(arch->GetSignature(), sig)) continue;

        auto* tcCol = arch->GetColumn(TypeManager::GetComponentTypeID<TimelineComponent>());
        auto* pbCol = arch->GetColumn(TypeManager::GetComponentTypeID<PlaybackComponent>());
        if (!tcCol || !pbCol) continue;

        for (size_t i = 0; i < arch->GetEntityCount(); ++i) {
            auto& tc = *static_cast<TimelineComponent*>(tcCol->Get(i));
            auto& pb = *static_cast<PlaybackComponent*>(pbCol->Get(i));

            if (!pb.playing) continue;

            tc.currentFrame = static_cast<int>(pb.currentSeconds * tc.fps);

            if (tc.currentFrame < tc.frameMin) tc.currentFrame = tc.frameMin;
            if (tc.currentFrame > tc.frameMax) tc.currentFrame = tc.frameMax;
        }
    }
}
