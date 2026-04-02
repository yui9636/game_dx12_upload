#include "TimelineAssetSerializer.h"
#include "TimelineAsset.h"
#include "JSONManager.h"
#include <fstream>

// ============================================================================
// JSON helpers for TimelineAsset types
// ============================================================================

static nlohmann::json ItemToJson(const TimelineItem& item, TimelineTrackType trackType)
{
    nlohmann::json j;
    j["startFrame"] = item.startFrame;
    j["endFrame"]   = item.endFrame;

    switch (trackType) {
    case TimelineTrackType::Hitbox:      j["hitbox"] = item.hitbox; break;
    case TimelineTrackType::VFX:         j["vfx"]    = item.vfx;   break;
    case TimelineTrackType::Audio:       j["audio"]  = item.audio;  break;
    case TimelineTrackType::CameraShake: j["shake"]  = item.shake;  break;
    case TimelineTrackType::Event:
        j["eventName"] = std::string(item.eventName);
        j["eventData"] = std::string(item.eventData);
        break;
    default: break;
    }
    return j;
}

static TimelineItem ItemFromJson(const nlohmann::json& j, TimelineTrackType trackType)
{
    TimelineItem item;
    item.startFrame = j.value("startFrame", 0);
    item.endFrame   = j.value("endFrame", 10);

    switch (trackType) {
    case TimelineTrackType::Hitbox:
        if (j.contains("hitbox")) j.at("hitbox").get_to(item.hitbox);
        break;
    case TimelineTrackType::VFX:
        if (j.contains("vfx")) j.at("vfx").get_to(item.vfx);
        break;
    case TimelineTrackType::Audio:
        if (j.contains("audio")) j.at("audio").get_to(item.audio);
        break;
    case TimelineTrackType::CameraShake:
        if (j.contains("shake")) j.at("shake").get_to(item.shake);
        break;
    case TimelineTrackType::Event: {
        std::string en = j.value("eventName", "");
        std::string ed = j.value("eventData", "");
        strncpy_s(item.eventName, en.c_str(), _TRUNCATE);
        strncpy_s(item.eventData, ed.c_str(), _TRUNCATE);
        break;
    }
    default: break;
    }
    return item;
}

static nlohmann::json KeyframeToJson(const TimelineKeyframe& kf)
{
    return {
        {"frame", kf.frame},
        {"value", { kf.value[0], kf.value[1], kf.value[2], kf.value[3] }},
        {"interpolation", static_cast<int>(kf.interpolation)}
    };
}

static TimelineKeyframe KeyframeFromJson(const nlohmann::json& j)
{
    TimelineKeyframe kf;
    kf.frame = j.value("frame", 0);
    if (j.contains("value") && j["value"].is_array()) {
        auto& arr = j["value"];
        for (int i = 0; i < 4 && i < (int)arr.size(); ++i)
            kf.value[i] = arr[i].get<float>();
    }
    kf.interpolation = static_cast<KeyframeInterpolation>(j.value("interpolation", 0));
    return kf;
}

// ============================================================================
// Save / Load
// ============================================================================

bool TimelineAssetSerializer::Save(const std::string& path, const TimelineAsset& asset)
{
    nlohmann::json root;
    root["name"]           = asset.name;
    root["fps"]            = asset.fps;
    root["duration"]       = asset.duration;
    root["ownerModelPath"] = asset.ownerModelPath;
    root["animationIndex"] = asset.animationIndex;
    root["nextTrackId"]    = asset.nextTrackId;

    auto& jTracks = root["tracks"];
    jTracks = nlohmann::json::array();

    for (auto& track : asset.tracks) {
        nlohmann::json jt;
        jt["id"]     = track.id;
        jt["name"]   = track.name;
        jt["type"]   = static_cast<int>(track.type);
        jt["muted"]  = track.muted;
        jt["locked"] = track.locked;
        jt["color"]  = track.color;

        jt["items"] = nlohmann::json::array();
        for (auto& item : track.items)
            jt["items"].push_back(ItemToJson(item, track.type));

        jt["keyframes"] = nlohmann::json::array();
        for (auto& kf : track.keyframes)
            jt["keyframes"].push_back(KeyframeToJson(kf));

        jTracks.push_back(std::move(jt));
    }

    std::ofstream ofs(path);
    if (!ofs.is_open()) return false;
    ofs << root.dump(2);
    return true;
}

bool TimelineAssetSerializer::Load(const std::string& path, TimelineAsset& outAsset)
{
    std::ifstream ifs(path);
    if (!ifs.is_open()) return false;

    nlohmann::json root;
    try { ifs >> root; }
    catch (...) { return false; }

    outAsset = {};
    outAsset.name           = root.value("name", "");
    outAsset.fps            = root.value("fps", 60.0f);
    outAsset.duration       = root.value("duration", 0.0f);
    outAsset.ownerModelPath = root.value("ownerModelPath", "");
    outAsset.animationIndex = root.value("animationIndex", -1);
    outAsset.nextTrackId    = root.value("nextTrackId", 1u);

    if (root.contains("tracks") && root["tracks"].is_array()) {
        for (auto& jt : root["tracks"]) {
            TimelineTrack track;
            track.id     = jt.value("id", 0u);
            track.name   = jt.value("name", "");
            track.type   = static_cast<TimelineTrackType>(jt.value("type", 0));
            track.muted  = jt.value("muted", false);
            track.locked = jt.value("locked", false);
            track.color  = jt.value("color", 0xFF808080u);

            if (jt.contains("items") && jt["items"].is_array())
                for (auto& ji : jt["items"])
                    track.items.push_back(ItemFromJson(ji, track.type));

            if (jt.contains("keyframes") && jt["keyframes"].is_array())
                for (auto& jk : jt["keyframes"])
                    track.keyframes.push_back(KeyframeFromJson(jk));

            outAsset.tracks.push_back(std::move(track));
        }
    }

    return true;
}
