#include "TimelineSystem.h"
#include "TimelineComponent.h"
#include "PlaybackComponent.h"
#include "Registry/Registry.h"
#include "Component/ComponentSignature.h"
#include "Type/TypeInfo.h"
#include "Archetype/Archetype.h"
#include <algorithm>

void TimelineSystem::Update(Registry& registry) {
    Signature sig = CreateSignature<TimelineComponent, PlaybackComponent>();
    for (auto* arch : registry.GetAllArchetypes()) {
        if (!SignatureMatches(arch->GetSignature(), sig)) continue;
        auto* tlCol = arch->GetColumn(TypeManager::GetComponentTypeID<TimelineComponent>());
        auto* playCol = arch->GetColumn(TypeManager::GetComponentTypeID<PlaybackComponent>());
        if (!tlCol || !playCol) continue;

        for (size_t i = 0; i < arch->GetEntityCount(); ++i) {
            auto& tl = *static_cast<TimelineComponent*>(tlCol->Get(i));
            auto& pb = *static_cast<PlaybackComponent*>(playCol->Get(i));

            // 秒を sequencer の規約に合わせて丸めつつ frame へ変換する。
            int frame = static_cast<int>(pb.currentSeconds * tl.fps + 0.5f);
            tl.currentFrame = std::clamp(frame, tl.frameMin, tl.frameMax);
            tl.playing = pb.playing;
            tl.clipLengthSec = pb.clipLength;
        }
    }
}
