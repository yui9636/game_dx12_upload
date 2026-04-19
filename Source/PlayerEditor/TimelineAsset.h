#pragma once
#include <string>
#include <vector>
#include <cstdint>
#include <DirectXMath.h>
#include "Storage/GameplayAsset.h"

// ============================================================================
// Timeline Track Types
// ============================================================================

enum class TimelineTrackType : uint8_t
{
    Animation = 0,
    Hitbox,
    VFX,
    Audio,
    CameraShake,
    Camera,
    Event,
    Custom
};

// ============================================================================
// Timeline Item (range-based: hitbox window, vfx span, audio span, etc.)
// ============================================================================

struct TimelineItem
{
    int startFrame = 0;
    int endFrame   = 10;

    // Payload (only the relevant one is used based on parent track type)
    GEHitboxPayload      hitbox{};
    GEVfxPayload         vfx{};
    GEAudioPayload       audio{};
    GECameraShakePayload shake{};

    // Event payload
    char eventName[64] = {};
    char eventData[256] = {};
};

// ============================================================================
// Timeline Keyframe (point-based: camera position, event trigger)
// ============================================================================

enum class KeyframeInterpolation : uint8_t
{
    Step = 0,
    Linear,
    CatmullRom,
    Bezier
};

struct TimelineKeyframe
{
    int   frame = 0;
    float value[4] = { 0, 0, 0, 0 }; // Generic 4-component value (position, rotation, scalar)
    KeyframeInterpolation interpolation = KeyframeInterpolation::Linear;
};

// ============================================================================
// Camera Payload (for Camera track keyframes)
// ============================================================================

struct CameraPayload
{
    DirectX::XMFLOAT3 eye{ 0, 5, -10 };
    DirectX::XMFLOAT3 focus{ 0, 1, 0 };
    float fov = 45.0f;
};

// ============================================================================
// Timeline Track
// ============================================================================

struct TimelineTrack
{
    uint32_t          id    = 0;
    std::string       name;
    TimelineTrackType type  = TimelineTrackType::Hitbox;
    bool              muted  = false;
    bool              locked = false;
    uint32_t          color  = 0xFF00A0FF;

    // Range-based items (Hitbox, VFX, Audio, CameraShake)
    std::vector<TimelineItem>     items;

    // Point-based keyframes (Camera, Event, Animation markers)
    std::vector<TimelineKeyframe> keyframes;
};

// ============================================================================
// Timeline Asset (top-level document)
// ============================================================================

struct TimelineAsset
{
    uint32_t      id            = 0;
    std::string   name;
    float         fps           = 60.0f;
    float         duration      = 0.0f;   // seconds
    std::string   ownerModelPath;
    int           animationIndex = -1;

    std::vector<TimelineTrack> tracks;

    uint32_t nextTrackId = 1;

    // Helpers
    int   GetFrameCount() const { return static_cast<int>(duration * fps); }
    float FrameToSeconds(int frame) const { return frame / fps; }
    int   SecondsToFrame(float sec) const { return static_cast<int>(sec * fps); }

    uint32_t GenerateTrackId() { return nextTrackId++; }

    TimelineTrack* AddTrack(TimelineTrackType type, const std::string& trackName)
    {
        TimelineTrack t;
        t.id   = GenerateTrackId();
        t.name = trackName;
        t.type = type;

        switch (type) {
        case TimelineTrackType::Hitbox:      t.color = 0xFFFF4040; break;
        case TimelineTrackType::VFX:         t.color = 0xFF40FF40; break;
        case TimelineTrackType::Audio:       t.color = 0xFF4080FF; break;
        case TimelineTrackType::CameraShake: t.color = 0xFFFF8040; break;
        case TimelineTrackType::Camera:      t.color = 0xFFFFFF40; break;
        case TimelineTrackType::Event:       t.color = 0xFFFF40FF; break;
        default:                             t.color = 0xFF808080; break;
        }

        tracks.push_back(std::move(t));
        return &tracks.back();
    }

    void RemoveTrack(uint32_t trackId)
    {
        tracks.erase(
            std::remove_if(tracks.begin(), tracks.end(),
                [trackId](const TimelineTrack& t) { return t.id == trackId; }),
            tracks.end()
        );
    }
};
