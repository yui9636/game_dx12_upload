#pragma once
#include <string>
#include <vector>
#include <cstdint>
#include <DirectXMath.h>
#include "Storage/GameplayAsset.h"

// タイムライン上で扱うトラックの種類を表す。
enum class TimelineTrackType : uint8_t
{
    Animation = 0,
    Hitbox,
    VFX,
    Audio,
    CameraShake,
    Camera,
    Event,
    Custom,
    Projectile   // 弾幕ボレー（追加は末尾固定: 既存アセットの type 値を壊さない）
};

// ヒットボックス、VFX、音声など、開始フレームと終了フレームを持つ区間アイテム。
struct TimelineItem
{
    int startFrame = 0;
    int endFrame   = 10;

    // 親トラックの種類に対応するペイロードだけを使用する。
    GEHitboxPayload      hitbox{};
    GEVfxPayload         vfx{};
    GEAudioPayload       audio{};
    GECameraShakePayload shake{};
    GEProjectilePayload  projectile{};

    // Event トラックで発火するイベント名と任意データ。
    char eventName[64] = {};
    char eventData[256] = {};
};

// カメラ位置やイベント発火など、単一フレームに置くキーフレーム。
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
    float value[4] = { 0, 0, 0, 0 }; // 位置、回転、スカラー値などを共通で扱う 4 成分値。
    KeyframeInterpolation interpolation = KeyframeInterpolation::Linear;
};

// Camera トラックのキーフレームが保持する視点、注視点、画角。
struct CameraPayload
{
    DirectX::XMFLOAT3 eye{ 0, 5, -10 };
    DirectX::XMFLOAT3 focus{ 0, 1, 0 };
    float fov = 45.0f;
};

// 同じ種類のタイムラインアイテムやキーフレームを束ねるトラック。
struct TimelineTrack
{
    uint32_t          id    = 0;
    std::string       name;
    TimelineTrackType type  = TimelineTrackType::Hitbox;
    bool              muted  = false;
    bool              locked = false;
    uint32_t          color  = 0xFF00A0FF;

    // Hitbox、VFX、Audio、CameraShake で使う区間アイテム。
    std::vector<TimelineItem>     items;

    // Camera、Event、Animation marker で使う点キーフレーム。
    std::vector<TimelineKeyframe> keyframes;
};

// プレイヤーエディターで保存するタイムライン 1 本分の編集データ。
struct TimelineAsset
{
    uint32_t      id            = 0;
    std::string   name;
    float         fps           = 60.0f;
    float         duration      = 0.0f;   // 秒単位のタイムライン長。
    std::string   ownerModelPath;
    int           animationIndex = -1;

    std::vector<TimelineTrack> tracks;

    uint32_t nextTrackId = 1;

    // フレームと秒の相互変換を、アセットに設定された fps で行う。
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
        case TimelineTrackType::Projectile:  t.color = 0xFF00D0FF; break;
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
