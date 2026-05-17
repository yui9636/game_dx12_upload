#include "TimelineSystem.h"
#include "TimelineComponent.h"
#include "TimelineItemBuffer.h"
#include "PlaybackComponent.h"
#include "Engine/EngineKernel.h"
#include "PlayerEditor/TimelineAsset.h"
#include "Registry/Registry.h"
#include "Component/ComponentSignature.h"
#include "Type/TypeInfo.h"
#include "Archetype/Archetype.h"
#include <algorithm>

namespace
{
    constexpr int kRuntimeEventItemType = static_cast<int>(TimelineTrackType::Event);

    bool DidCrossFrame(const TimelineComponent& timeline, int targetFrame)
    {
        if (targetFrame < timeline.frameMin || targetFrame > timeline.frameMax) {
            return false;
        }

        if (timeline.previousFrame < 0) {
            return timeline.currentFrame >= targetFrame;
        }

        if (timeline.currentFrame < timeline.previousFrame) {
            return targetFrame > timeline.previousFrame || targetFrame <= timeline.currentFrame;
        }

        return targetFrame > timeline.previousFrame && targetFrame <= timeline.currentFrame;
    }

    void DispatchTimelineEvents(const TimelineComponent& timeline, TimelineItemBuffer& buffer)
    {
        if (!timeline.playing) {
            return;
        }

        const bool wrapped = timeline.previousFrame >= 0 && timeline.currentFrame < timeline.previousFrame;
        for (auto& item : buffer.items) {
            if (item.type != kRuntimeEventItemType) {
                continue;
            }

            if (wrapped || timeline.currentFrame < item.start) {
                item.fired = false;
            }

            if (!item.fired && DidCrossFrame(timeline, item.start)) {
                item.fired = true;
                if (item.eventName[0] != '\0') {
                    EngineKernel::Instance().GetFlowEventQueue().Push(item.eventName, item.eventData);
                }
            }
        }
    }
}

void TimelineSystem::Update(Registry& registry) {
    Signature sig = CreateSignature<TimelineComponent, PlaybackComponent>();
    for (auto* arch : registry.GetAllArchetypes()) {
        if (!SignatureMatches(arch->GetSignature(), sig)) continue;
        auto* tlCol = arch->GetColumn(TypeManager::GetComponentTypeID<TimelineComponent>());
        auto* playCol = arch->GetColumn(TypeManager::GetComponentTypeID<PlaybackComponent>());
        const auto bufferTypeId = TypeManager::GetComponentTypeID<TimelineItemBuffer>();
        auto* bufferCol = arch->GetSignature().test(bufferTypeId) ? arch->GetColumn(bufferTypeId) : nullptr;
        if (!tlCol || !playCol) continue;

        for (size_t i = 0; i < arch->GetEntityCount(); ++i) {
            auto& tl = *static_cast<TimelineComponent*>(tlCol->Get(i));
            auto& pb = *static_cast<PlaybackComponent*>(playCol->Get(i));

            // 秒を sequencer の規約に合わせて丸めつつ frame へ変換する。
            const int previousEvaluatedFrame = tl.previousFrame;
            int frame = static_cast<int>(pb.currentSeconds * tl.fps + 0.5f);
            tl.currentFrame = std::clamp(frame, tl.frameMin, tl.frameMax);
            tl.playing = pb.playing;
            tl.clipLengthSec = pb.clipLength;

            if (bufferCol) {
                TimelineComponent eventTimeline = tl;
                eventTimeline.previousFrame = previousEvaluatedFrame;
                DispatchTimelineEvents(eventTimeline, *static_cast<TimelineItemBuffer*>(bufferCol->Get(i)));
            }

            tl.previousFrame = tl.currentFrame;
        }
    }
}
