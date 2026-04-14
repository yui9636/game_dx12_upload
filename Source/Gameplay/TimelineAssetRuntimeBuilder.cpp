#include "TimelineAssetRuntimeBuilder.h"

#include "Gameplay/TimelineComponent.h"
#include "Gameplay/TimelineItemBuffer.h"
#include "PlayerEditor/TimelineAsset.h"
#include "PlayerEditor/TimelineAssetSerializer.h"

#include <algorithm>
#include <unordered_map>

namespace
{
    std::unordered_map<std::string, TimelineAsset> s_timelineAssetCache;

    void ResetTimelineOutputs(TimelineComponent& outTimeline, TimelineItemBuffer& outBuffer)
    {
        outBuffer.items.clear();
        outTimeline = TimelineComponent{};
        outTimeline.fps = 60.0f;
        outTimeline.frameMin = 0;
        outTimeline.frameMax = 0;
        outTimeline.clipLengthSec = 0.0f;
        outTimeline.playing = true;
    }

    int ResolveTimelineFrameMax(const TimelineAsset& asset)
    {
        int frameMax = (std::max)(0, asset.GetFrameCount());
        for (const auto& track : asset.tracks) {
            for (const auto& item : track.items) {
                frameMax = (std::max)(frameMax, item.endFrame);
            }
            for (const auto& keyframe : track.keyframes) {
                frameMax = (std::max)(frameMax, keyframe.frame);
            }
        }
        return frameMax;
    }

    uint32_t TrackTypeMask(TimelineTrackType type)
    {
        return 1u << static_cast<uint32_t>(type);
    }

    const TimelineAsset* GetCachedTimelineAsset(const std::string& path)
    {
        if (path.empty()) {
            return nullptr;
        }

        auto it = s_timelineAssetCache.find(path);
        if (it != s_timelineAssetCache.end()) {
            return &it->second;
        }

        TimelineAsset asset;
        if (!TimelineAssetSerializer::Load(path, asset)) {
            return nullptr;
        }

        s_timelineAssetCache[path] = std::move(asset);
        return &s_timelineAssetCache[path];
    }
}

namespace TimelineAssetRuntimeBuilder
{
    bool Build(
        const TimelineAsset& asset,
        int animationIndex,
        TimelineComponent& outTimeline,
        TimelineItemBuffer& outBuffer,
        bool* outPartialBuild,
        uint32_t* outWarningCount,
        uint32_t* outUnsupportedTrackMask)
    {
        ResetTimelineOutputs(outTimeline, outBuffer);
        bool partialBuild = false;
        uint32_t warningCount = 0;
        uint32_t unsupportedTrackMask = 0;

        outTimeline.animationIndex = animationIndex >= 0 ? animationIndex : asset.animationIndex;
        outTimeline.fps = asset.fps > 0.0f ? asset.fps : 60.0f;
        outTimeline.frameMin = 0;
        outTimeline.frameMax = ResolveTimelineFrameMax(asset);
        outTimeline.clipLengthSec = asset.duration > 0.0f
            ? asset.duration
            : (outTimeline.frameMax > 0 && outTimeline.fps > 0.0f
                ? static_cast<float>(outTimeline.frameMax) / outTimeline.fps
                : 0.0f);

        for (const auto& track : asset.tracks) {
            if (track.muted) {
                continue;
            }

            for (const auto& item : track.items) {
                GESequencerItem runtimeItem;
                runtimeItem.start = item.startFrame;
                runtimeItem.end = item.endFrame;
                runtimeItem.color = track.color;
                runtimeItem.label = track.name;

                switch (track.type) {
                case TimelineTrackType::Hitbox:
                    runtimeItem.type = 0;
                    runtimeItem.hb = item.hitbox;
                    break;
                case TimelineTrackType::VFX:
                    runtimeItem.type = 2;
                    runtimeItem.vfx = item.vfx;
                    break;
                case TimelineTrackType::Audio:
                    runtimeItem.type = 3;
                    runtimeItem.audio = item.audio;
                    break;
                case TimelineTrackType::CameraShake:
                    runtimeItem.type = 4;
                    runtimeItem.shake = item.shake;
                    break;
                default:
                    partialBuild = true;
                    ++warningCount;
                    unsupportedTrackMask |= TrackTypeMask(track.type);
                    continue;
                }

                outBuffer.items.push_back(std::move(runtimeItem));
            }

            if (!track.keyframes.empty()) {
                switch (track.type) {
                case TimelineTrackType::Animation:
                case TimelineTrackType::Camera:
                case TimelineTrackType::Event:
                case TimelineTrackType::Custom:
                    partialBuild = true;
                    ++warningCount;
                    unsupportedTrackMask |= TrackTypeMask(track.type);
                    break;
                default:
                    break;
                }
            }
        }

        if (outPartialBuild) {
            *outPartialBuild = partialBuild;
        }
        if (outWarningCount) {
            *outWarningCount = warningCount;
        }
        if (outUnsupportedTrackMask) {
            *outUnsupportedTrackMask = unsupportedTrackMask;
        }
        return true;
    }

    bool BuildFromPath(
        const std::string& path,
        int animationIndex,
        TimelineComponent& outTimeline,
        TimelineItemBuffer& outBuffer,
        bool* outPartialBuild,
        uint32_t* outWarningCount,
        uint32_t* outUnsupportedTrackMask)
    {
        ResetTimelineOutputs(outTimeline, outBuffer);

        const TimelineAsset* asset = GetCachedTimelineAsset(path);
        if (!asset) {
            if (outPartialBuild) {
                *outPartialBuild = false;
            }
            if (outWarningCount) {
                *outWarningCount = 0;
            }
            if (outUnsupportedTrackMask) {
                *outUnsupportedTrackMask = 0;
            }
            return false;
        }

        return Build(*asset, animationIndex, outTimeline, outBuffer, outPartialBuild, outWarningCount, outUnsupportedTrackMask);
    }

    void InvalidateAssetCache(const char* path)
    {
        if (!path || path[0] == '\0') {
            s_timelineAssetCache.clear();
            return;
        }

        s_timelineAssetCache.erase(path);
    }
}
