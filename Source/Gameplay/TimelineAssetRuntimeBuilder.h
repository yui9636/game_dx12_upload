#pragma once

#include <cstdint>
#include "Gameplay/TimelineComponent.h"
#include "Gameplay/TimelineItemBuffer.h"
#include "PlayerEditor/TimelineAsset.h"

namespace TimelineAssetRuntimeBuilder
{
    // Build は編集用 TimelineAsset を実行時 TimelineComponent と連続バッファへ展開する。
    bool Build(
        const TimelineAsset& asset,
        int animationIndex,
        TimelineComponent& outTimeline,
        TimelineItemBuffer& outBuffer,
        bool* outPartialBuild = nullptr,
        uint32_t* outWarningCount = nullptr,
        uint32_t* outUnsupportedTrackMask = nullptr);

    // InvalidateAssetCache は指定アセットまたは全体の変換キャッシュを破棄し、次回 Build で再構築させる。
    void InvalidateAssetCache(const char* path = nullptr);
}
