#pragma once

#include <cstdint>
#include "Gameplay/TimelineComponent.h"
#include "Gameplay/TimelineItemBuffer.h"
#include "PlayerEditor/TimelineAsset.h"

namespace TimelineAssetRuntimeBuilder
{
    bool Build(
        const TimelineAsset& asset,
        int animationIndex,
        TimelineComponent& outTimeline,
        TimelineItemBuffer& outBuffer,
        bool* outPartialBuild = nullptr,
        uint32_t* outWarningCount = nullptr,
        uint32_t* outUnsupportedTrackMask = nullptr);

    void InvalidateAssetCache(const char* path = nullptr);
}
