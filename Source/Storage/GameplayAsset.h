#pragma once

#include <string>
#include <cstdint>
#include <vector>
#include <DirectXMath.h>
#include "Utils/JSONManager.h" 
#include "EffectRuntime/EffectService.h"

namespace Effekseer { class Effect; }
struct GEHitboxPayload
{
    int                   nodeIndex = 0;
    DirectX::XMFLOAT3     offsetLocal{ 0.0f, 0.0f, 0.0f };
    float                 radius = 30.0f;
    unsigned int          rgba = 0x40FF0000;
};

// NLOHMANN_DEFINE_TYPE_INTRUSIVE を使わず手書きにし、後から field が増えても
// 古い asset 読み込み時に自然に既定値へ落ちるようにする。
inline void to_json(nlohmann::json& j, const GEHitboxPayload& p) {
    j = nlohmann::json{
        {"nodeIndex", p.nodeIndex},
        {"offsetLocal", p.offsetLocal},
        {"radius", p.radius},
        {"rgba", p.rgba}
    };
}
inline void from_json(const nlohmann::json& j, GEHitboxPayload& p) {
    p.nodeIndex   = j.value("nodeIndex", 0);
    p.offsetLocal = j.value("offsetLocal", DirectX::XMFLOAT3{ 0.0f, 0.0f, 0.0f });
    p.radius      = j.value("radius", 30.0f);
    p.rgba        = j.value("rgba", 0x40FF0000u);
}

struct GEVfxPayload
{
    char                  assetId[128] = { 0 };
    int                   nodeIndex = -1;
    DirectX::XMFLOAT3     offsetLocal{ 0.0f, 0.0f, 0.0f };
    DirectX::XMFLOAT3     offsetRotDeg{ 0.0f, 0.0f, 0.0f };
    DirectX::XMFLOAT3     offsetScale{ 1.0f, 1.0f, 1.0f };
    bool                  fireOnEnterOnly = true;
};

inline void to_json(nlohmann::json& j, const GEVfxPayload& p) {
    j = nlohmann::json{
        {"assetId", std::string(p.assetId)},
        {"nodeIndex", p.nodeIndex},
        {"offsetLocal", p.offsetLocal},
        {"offsetRotDeg", p.offsetRotDeg},
        {"offsetScale", p.offsetScale},
        {"fireOnEnterOnly", p.fireOnEnterOnly}
    };
}
inline void from_json(const nlohmann::json& j, GEVfxPayload& p) {
    std::string s = j.value("assetId", "");
    strncpy_s(p.assetId, s.c_str(), _TRUNCATE);
    p.nodeIndex = j.value("nodeIndex", -1);
    p.offsetLocal = j.value("offsetLocal", DirectX::XMFLOAT3{ 0,0,0 });
    p.offsetRotDeg = j.value("offsetRotDeg", DirectX::XMFLOAT3{ 0,0,0 });
    p.offsetScale = j.value("offsetScale", DirectX::XMFLOAT3{ 1,1,1 });
    p.fireOnEnterOnly = j.value("fireOnEnterOnly", true);
}

struct GEAudioPayload
{
    char  assetId[260] = { 0 };
    float volume = 1.0f;
    float pitch = 1.0f;
    bool  is3D = false;
    bool  loop = false;
};

inline void to_json(nlohmann::json& j, const GEAudioPayload& p) {
    j = nlohmann::json{
        {"assetId", std::string(p.assetId)},
        {"volume", p.volume},
        {"pitch", p.pitch},
        {"is3D", p.is3D},
        {"loop", p.loop}
    };
}
inline void from_json(const nlohmann::json& j, GEAudioPayload& p) {
    std::string s = j.value("assetId", "");
    strncpy_s(p.assetId, s.c_str(), _TRUNCATE);
    p.volume = j.value("volume", 1.0f);
    p.pitch = j.value("pitch", 1.0f);
    p.is3D = j.value("is3D", false);
    p.loop = j.value("loop", false);
}

struct GECameraShakePayload
{
    float duration = 0.2f;
    float amplitude = 0.5f;
    float frequency = 20.0f;
    float decay = 0.9f;

    float hitStopDuration = 0.0f;
    float timeScale = 0.0f;
};

inline void to_json(nlohmann::json& j, const GECameraShakePayload& p) {
    j = nlohmann::json{
        {"duration", p.duration}, {"amplitude", p.amplitude},
        {"frequency", p.frequency}, {"decay", p.decay},
        {"hitStopDuration", p.hitStopDuration}, {"timeScale", p.timeScale}
    };
}
inline void from_json(const nlohmann::json& j, GECameraShakePayload& p) {
    p.duration = j.value("duration", 0.2f);
    p.amplitude = j.value("amplitude", 0.5f);
    p.frequency = j.value("frequency", 20.0f);
    p.decay = j.value("decay", 0.9f);
    p.hitStopDuration = j.value("hitStopDuration", 0.0f);
    p.timeScale = j.value("timeScale", 0.0f);
}

// 弾幕発射 1 ボレー分の設定。Projectile トラックのアイテムが保持する。
struct GEProjectilePayload
{
    int   pattern          = 0;     // 0=Aimed, 1=Spread, 2=Ring
    int   bulletsPerVolley = 8;
    float spreadAngleDeg   = 60.0f;
    float bulletSpeed      = 9.0f;
    float bulletLifetime   = 5.0f;
    int   bulletDamage     = 8;
    float bulletRadius     = 0.35f;
    float bulletScale      = 0.3f;
    bool  targetsPlayer    = true;
    DirectX::XMFLOAT3 offsetLocal{ 0.0f, 1.2f, 0.0f };
    char  bulletModelPath[256] = {};
};

inline void to_json(nlohmann::json& j, const GEProjectilePayload& p) {
    j = nlohmann::json{
        {"pattern", p.pattern},
        {"bulletsPerVolley", p.bulletsPerVolley},
        {"spreadAngleDeg", p.spreadAngleDeg},
        {"bulletSpeed", p.bulletSpeed},
        {"bulletLifetime", p.bulletLifetime},
        {"bulletDamage", p.bulletDamage},
        {"bulletRadius", p.bulletRadius},
        {"bulletScale", p.bulletScale},
        {"targetsPlayer", p.targetsPlayer},
        {"offsetLocal", p.offsetLocal},
        {"bulletModelPath", std::string(p.bulletModelPath)}
    };
}
inline void from_json(const nlohmann::json& j, GEProjectilePayload& p) {
    p.pattern          = j.value("pattern", 0);
    p.bulletsPerVolley = j.value("bulletsPerVolley", 8);
    p.spreadAngleDeg   = j.value("spreadAngleDeg", 60.0f);
    p.bulletSpeed      = j.value("bulletSpeed", 9.0f);
    p.bulletLifetime   = j.value("bulletLifetime", 5.0f);
    p.bulletDamage     = j.value("bulletDamage", 8);
    p.bulletRadius     = j.value("bulletRadius", 0.35f);
    p.bulletScale      = j.value("bulletScale", 0.3f);
    p.targetsPlayer    = j.value("targetsPlayer", true);
    p.offsetLocal      = j.value("offsetLocal", DirectX::XMFLOAT3{ 0.0f, 1.2f, 0.0f });
    const std::string model = j.value("bulletModelPath", "");
    strncpy_s(p.bulletModelPath, model.c_str(), _TRUNCATE);
}

struct GESequencerItem
{
    int          type = 0;
    int          start = 0;
    int          end = 0;
    unsigned int color = 0xFF00A0FF;
    std::string  label;

    GEHitboxPayload hb;
    GEVfxPayload    vfx;
    GEAudioPayload  audio;
    GECameraShakePayload shake;
    GEProjectilePayload  proj;
    char eventName[64] = {};
    char eventData[256] = {};

    bool                         vfxActive = false;
    EffectHandle                 vfxHandle;
    bool                         audioActive = false;
    uint64_t                     audioHandle = 0;

    bool fired = false;
};

inline void to_json(nlohmann::json& j, const GESequencerItem& p) {
    j = nlohmann::json{
        {"type", p.type},
        {"start", p.start},
        {"end", p.end},
        {"color", p.color},
        {"label", p.label},
        {"hb", p.hb},
        {"vfx", p.vfx},
        {"audio", p.audio},
        {"shake", p.shake},
        {"proj", p.proj},
        {"eventName", std::string(p.eventName)},
        {"eventData", std::string(p.eventData)}
    };
}
inline void from_json(const nlohmann::json& j, GESequencerItem& p) {
    p.type = j.value("type", 0);
    p.start = j.value("start", 0);
    p.end = j.value("end", 0);
    p.color = j.value("color", 0xFF00A0FF);
    p.label = j.value("label", "Label");

    if (j.contains("hb")) j.at("hb").get_to(p.hb);
    if (j.contains("vfx")) j.at("vfx").get_to(p.vfx);
    if (j.contains("audio")) j.at("audio").get_to(p.audio);
    if (j.contains("shake")) j.at("shake").get_to(p.shake);
    if (j.contains("proj")) j.at("proj").get_to(p.proj);
    const std::string eventName = j.value("eventName", "");
    const std::string eventData = j.value("eventData", "");
    strncpy_s(p.eventName, eventName.c_str(), _TRUNCATE);
    strncpy_s(p.eventData, eventData.c_str(), _TRUNCATE);

 
}

struct GECurvePoint
{
    float x = 0.0f;
    float y = 1.0f;
    NLOHMANN_DEFINE_TYPE_INTRUSIVE(GECurvePoint, x, y)
};

struct GECurveSettings
{
    bool enabled = false;
    bool useRange = false;
    std::vector<GECurvePoint> points;
    NLOHMANN_DEFINE_TYPE_INTRUSIVE(GECurveSettings, enabled, useRange, points)
};

struct GameplayAsset
{
    std::vector<std::vector<GESequencerItem>> timelines;
    std::vector<GECurveSettings> curves;
};

inline void to_json(nlohmann::json& j, const GameplayAsset& p) {
    j = nlohmann::json{
        {"timelines", p.timelines},
        {"curves", p.curves}
    };
}
inline void from_json(const nlohmann::json& j, GameplayAsset& p) {
    if (j.contains("timelines")) j.at("timelines").get_to(p.timelines);
    if (j.contains("curves")) j.at("curves").get_to(p.curves);
    else p.curves.resize(p.timelines.size());
}
