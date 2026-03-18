#pragma once

#include <string>
#include <vector>
#include <DirectXMath.h>
#include "JSONManager.h" 

namespace Effekseer { class Effect; }
class EffectInstance;
class AudioSource;

// ============================================================================
// データ構造定義
// ============================================================================

struct GEHitboxPayload
{
    int                   nodeIndex = 0;
    DirectX::XMFLOAT3     offsetLocal{ 0.0f, 0.0f, 0.0f };
    float                 radius = 30.0f;
    unsigned int          rgba = 0x40FF0000;

    NLOHMANN_DEFINE_TYPE_INTRUSIVE(GEHitboxPayload, nodeIndex, offsetLocal, radius, rgba)
};

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
    char  assetId[128] = { 0 };
    float volume = 1.0f;
    float pitch = 1.0f;
    bool  is3D = false;
    int   nodeIndex = -1;
    bool  loop = false;
};

inline void to_json(nlohmann::json& j, const GEAudioPayload& p) {
    j = nlohmann::json{
        {"assetId", std::string(p.assetId)},
        {"volume", p.volume},
        {"pitch", p.pitch},
        {"is3D", p.is3D},
        {"nodeIndex", p.nodeIndex},
        {"loop", p.loop}
    };
}
inline void from_json(const nlohmann::json& j, GEAudioPayload& p) {
    std::string s = j.value("assetId", "");
    strncpy_s(p.assetId, s.c_str(), _TRUNCATE);
    p.volume = j.value("volume", 1.0f);
    p.pitch = j.value("pitch", 1.0f);
    p.is3D = j.value("is3D", false);
    p.nodeIndex = j.value("nodeIndex", -1);
    p.loop = j.value("loop", false);
}

// シェイク & ヒットストップ用 (ワンショットイベント)
struct GECameraShakePayload
{
    float duration = 0.2f;      // シェイク時間
    float amplitude = 0.5f;     // 強さ
    float frequency = 20.0f;    // 振動数
    float decay = 0.9f;         // 減衰率

    float hitStopDuration = 0.0f; // ヒットストップ時間
    float timeScale = 0.0f;       // ストップ中の速度
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

// シーケンサーアイテム
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

    // ランタイム変数
    bool               vfxActive = false;
    std::shared_ptr<EffectInstance> vfxInstance;
    bool                         audioActive = false;
    std::shared_ptr<AudioSource> audioSource;

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
        // "cam" は削除
        {"audio", p.audio},
        {"shake", p.shake}
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