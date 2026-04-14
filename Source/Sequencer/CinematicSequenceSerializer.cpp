#include "CinematicSequenceSerializer.h"

#include "CinematicSequenceAsset.h"
#include "JSONManager.h"

#include <filesystem>
#include <fstream>

namespace
{
    nlohmann::json ToJson(const CinematicScalarOverride& value)
    {
        return {
            {"name", value.name},
            {"value", value.value}
        };
    }

    CinematicScalarOverride ScalarOverrideFromJson(const nlohmann::json& json)
    {
        CinematicScalarOverride value;
        value.name = json.value("name", "");
        value.value = json.value("value", 0.0f);
        return value;
    }

    nlohmann::json ToJson(const CinematicColorOverride& value)
    {
        return {
            {"name", value.name},
            {"value", value.value}
        };
    }

    CinematicColorOverride ColorOverrideFromJson(const nlohmann::json& json)
    {
        CinematicColorOverride value;
        value.name = json.value("name", "");
        value.value = json.value("value", DirectX::XMFLOAT4{ 1.0f, 1.0f, 1.0f, 1.0f });
        return value;
    }

    nlohmann::json ToJson(const CinematicTransformSection& value)
    {
        return {
            {"startPosition", value.startPosition},
            {"endPosition", value.endPosition},
            {"startRotationEuler", value.startRotationEuler},
            {"endRotationEuler", value.endRotationEuler},
            {"startScale", value.startScale},
            {"endScale", value.endScale}
        };
    }

    CinematicTransformSection TransformFromJson(const nlohmann::json& json)
    {
        CinematicTransformSection value;
        value.startPosition = json.value("startPosition", DirectX::XMFLOAT3{ 0.0f, 0.0f, 0.0f });
        value.endPosition = json.value("endPosition", DirectX::XMFLOAT3{ 0.0f, 0.0f, 0.0f });
        value.startRotationEuler = json.value("startRotationEuler", DirectX::XMFLOAT3{ 0.0f, 0.0f, 0.0f });
        value.endRotationEuler = json.value("endRotationEuler", DirectX::XMFLOAT3{ 0.0f, 0.0f, 0.0f });
        value.startScale = json.value("startScale", DirectX::XMFLOAT3{ 1.0f, 1.0f, 1.0f });
        value.endScale = json.value("endScale", DirectX::XMFLOAT3{ 1.0f, 1.0f, 1.0f });
        return value;
    }

    nlohmann::json ToJson(const CinematicCameraSection& value)
    {
        return {
            {"cameraBindingId", value.cameraBindingId},
            {"cameraMode", static_cast<int>(value.cameraMode)},
            {"startPosition", value.startPosition},
            {"endPosition", value.endPosition},
            {"startRotationEuler", value.startRotationEuler},
            {"endRotationEuler", value.endRotationEuler},
            {"startEye", value.startEye},
            {"endEye", value.endEye},
            {"startTarget", value.startTarget},
            {"endTarget", value.endTarget},
            {"startFovDeg", value.startFovDeg},
            {"endFovDeg", value.endFovDeg},
            {"startRollDeg", value.startRollDeg},
            {"endRollDeg", value.endRollDeg},
            {"startFocusDistance", value.startFocusDistance},
            {"endFocusDistance", value.endFocusDistance},
            {"startAperture", value.startAperture},
            {"endAperture", value.endAperture},
            {"blendEaseIn", value.blendEaseIn},
            {"blendEaseOut", value.blendEaseOut}
        };
    }

    CinematicCameraSection CameraFromJson(const nlohmann::json& json)
    {
        CinematicCameraSection value;
        value.cameraBindingId = json.value("cameraBindingId", 0ull);
        value.cameraMode = static_cast<CinematicCameraMode>(json.value("cameraMode", 0));
        value.startPosition = json.value("startPosition", DirectX::XMFLOAT3{ 0.0f, 2.0f, -10.0f });
        value.endPosition = json.value("endPosition", DirectX::XMFLOAT3{ 0.0f, 2.0f, -10.0f });
        value.startRotationEuler = json.value("startRotationEuler", DirectX::XMFLOAT3{ 0.0f, 0.0f, 0.0f });
        value.endRotationEuler = json.value("endRotationEuler", DirectX::XMFLOAT3{ 0.0f, 0.0f, 0.0f });
        value.startEye = json.value("startEye", DirectX::XMFLOAT3{ 0.0f, 2.0f, -10.0f });
        value.endEye = json.value("endEye", DirectX::XMFLOAT3{ 0.0f, 2.0f, -10.0f });
        value.startTarget = json.value("startTarget", DirectX::XMFLOAT3{ 0.0f, 1.0f, 0.0f });
        value.endTarget = json.value("endTarget", DirectX::XMFLOAT3{ 0.0f, 1.0f, 0.0f });
        value.startFovDeg = json.value("startFovDeg", 45.0f);
        value.endFovDeg = json.value("endFovDeg", 45.0f);
        value.startRollDeg = json.value("startRollDeg", 0.0f);
        value.endRollDeg = json.value("endRollDeg", 0.0f);
        value.startFocusDistance = json.value("startFocusDistance", 500.0f);
        value.endFocusDistance = json.value("endFocusDistance", 500.0f);
        value.startAperture = json.value("startAperture", 2.8f);
        value.endAperture = json.value("endAperture", 2.8f);
        value.blendEaseIn = json.value("blendEaseIn", 0.0f);
        value.blendEaseOut = json.value("blendEaseOut", 0.0f);
        return value;
    }

    nlohmann::json ToJson(const CinematicAnimationSection& value)
    {
        return {
            {"animationIndex", value.animationIndex},
            {"animationName", value.animationName},
            {"loop", value.loop},
            {"playRate", value.playRate},
            {"blendInFrames", value.blendInFrames},
            {"blendOutFrames", value.blendOutFrames},
            {"slot", value.slot}
        };
    }

    CinematicAnimationSection AnimationFromJson(const nlohmann::json& json)
    {
        CinematicAnimationSection value;
        value.animationIndex = json.value("animationIndex", -1);
        value.animationName = json.value("animationName", "");
        value.loop = json.value("loop", true);
        value.playRate = json.value("playRate", 1.0f);
        value.blendInFrames = json.value("blendInFrames", 6.0f);
        value.blendOutFrames = json.value("blendOutFrames", 6.0f);
        value.slot = json.value("slot", "Base");
        return value;
    }

    nlohmann::json ToJson(const CinematicEffectSection& value)
    {
        nlohmann::json root;
        root["effectAssetPath"] = value.effectAssetPath;
        root["socketName"] = value.socketName;
        root["offsetPosition"] = value.offsetPosition;
        root["offsetRotation"] = value.offsetRotation;
        root["offsetScale"] = value.offsetScale;
        root["seed"] = value.seed;
        root["loop"] = value.loop;
        root["fireOnEnterOnly"] = value.fireOnEnterOnly;
        root["stopOnExit"] = value.stopOnExit;
        root["retriggerPolicy"] = static_cast<int>(value.retriggerPolicy);
        root["assetOverrides"] = nlohmann::json::array();
        root["editorPreviewScalarOverrides"] = nlohmann::json::array();
        root["colorOverrides"] = nlohmann::json::array();
        for (const auto& overrideValue : value.assetOverrides) {
            root["assetOverrides"].push_back(ToJson(overrideValue));
        }
        for (const auto& overrideValue : value.editorPreviewScalarOverrides) {
            root["editorPreviewScalarOverrides"].push_back(ToJson(overrideValue));
        }
        for (const auto& overrideValue : value.colorOverrides) {
            root["colorOverrides"].push_back(ToJson(overrideValue));
        }
        return root;
    }

    CinematicEffectSection EffectFromJson(const nlohmann::json& json)
    {
        CinematicEffectSection value;
        value.effectAssetPath = json.value("effectAssetPath", "");
        value.socketName = json.value("socketName", "");
        value.offsetPosition = json.value("offsetPosition", DirectX::XMFLOAT3{ 0.0f, 0.0f, 0.0f });
        value.offsetRotation = json.value("offsetRotation", DirectX::XMFLOAT3{ 0.0f, 0.0f, 0.0f });
        value.offsetScale = json.value("offsetScale", DirectX::XMFLOAT3{ 1.0f, 1.0f, 1.0f });
        value.seed = json.value("seed", 1u);
        value.loop = json.value("loop", false);
        value.fireOnEnterOnly = json.value("fireOnEnterOnly", true);
        value.stopOnExit = json.value("stopOnExit", true);
        value.retriggerPolicy = static_cast<CinematicRetriggerPolicy>(json.value("retriggerPolicy", 0));
        if (json.contains("assetOverrides") && json["assetOverrides"].is_array()) {
            for (const auto& item : json["assetOverrides"]) {
                value.assetOverrides.push_back(ScalarOverrideFromJson(item));
            }
        }
        if (json.contains("editorPreviewScalarOverrides") && json["editorPreviewScalarOverrides"].is_array()) {
            for (const auto& item : json["editorPreviewScalarOverrides"]) {
                value.editorPreviewScalarOverrides.push_back(ScalarOverrideFromJson(item));
            }
        }
        if (json.contains("colorOverrides") && json["colorOverrides"].is_array()) {
            for (const auto& item : json["colorOverrides"]) {
                value.colorOverrides.push_back(ColorOverrideFromJson(item));
            }
        }
        return value;
    }

    nlohmann::json ToJson(const CinematicAudioSection& value)
    {
        return {
            {"audioAssetPath", value.audioAssetPath},
            {"is3D", value.is3D},
            {"attachSocket", value.attachSocket},
            {"volume", value.volume},
            {"pitch", value.pitch},
            {"loop", value.loop},
            {"startOffsetSec", value.startOffsetSec},
            {"stopOnExit", value.stopOnExit},
            {"retriggerPolicy", static_cast<int>(value.retriggerPolicy)}
        };
    }

    CinematicAudioSection AudioFromJson(const nlohmann::json& json)
    {
        CinematicAudioSection value;
        value.audioAssetPath = json.value("audioAssetPath", "");
        value.is3D = json.value("is3D", true);
        value.attachSocket = json.value("attachSocket", "");
        value.volume = json.value("volume", 1.0f);
        value.pitch = json.value("pitch", 1.0f);
        value.loop = json.value("loop", false);
        value.startOffsetSec = json.value("startOffsetSec", 0.0f);
        value.stopOnExit = json.value("stopOnExit", true);
        value.retriggerPolicy = static_cast<CinematicRetriggerPolicy>(json.value("retriggerPolicy", 0));
        return value;
    }

    nlohmann::json ToJson(const CinematicEventSection& value)
    {
        return {
            {"eventName", value.eventName},
            {"eventCategory", value.eventCategory},
            {"payloadType", value.payloadType},
            {"payloadJson", value.payloadJson},
            {"fireOnce", value.fireOnce}
        };
    }

    CinematicEventSection EventFromJson(const nlohmann::json& json)
    {
        CinematicEventSection value;
        value.eventName = json.value("eventName", "");
        value.eventCategory = json.value("eventCategory", "");
        value.payloadType = json.value("payloadType", "String");
        value.payloadJson = json.value("payloadJson", "");
        value.fireOnce = json.value("fireOnce", true);
        return value;
    }

    nlohmann::json ToJson(const CinematicCameraShakeSection& value)
    {
        return {
            {"duration", value.duration},
            {"amplitude", value.amplitude},
            {"frequency", value.frequency},
            {"decay", value.decay},
            {"hitStopDuration", value.hitStopDuration},
            {"timeScale", value.timeScale}
        };
    }

    CinematicCameraShakeSection CameraShakeFromJson(const nlohmann::json& json)
    {
        CinematicCameraShakeSection value;
        value.duration = json.value("duration", 0.2f);
        value.amplitude = json.value("amplitude", 0.5f);
        value.frequency = json.value("frequency", 20.0f);
        value.decay = json.value("decay", 0.9f);
        value.hitStopDuration = json.value("hitStopDuration", 0.0f);
        value.timeScale = json.value("timeScale", 0.0f);
        return value;
    }

    nlohmann::json ToJson(const CinematicSection& section)
    {
        return {
            {"sectionId", section.sectionId},
            {"trackType", static_cast<int>(section.trackType)},
            {"label", section.label},
            {"startFrame", section.startFrame},
            {"endFrame", section.endFrame},
            {"rowIndex", section.rowIndex},
            {"muted", section.muted},
            {"locked", section.locked},
            {"evalPolicy", static_cast<int>(section.evalPolicy)},
            {"seekPolicy", static_cast<int>(section.seekPolicy)},
            {"easeInFrames", section.easeInFrames},
            {"easeOutFrames", section.easeOutFrames},
            {"color", section.color},
            {"transform", ToJson(section.transform)},
            {"camera", ToJson(section.camera)},
            {"animation", ToJson(section.animation)},
            {"effect", ToJson(section.effect)},
            {"audio", ToJson(section.audio)},
            {"eventData", ToJson(section.eventData)},
            {"shake", ToJson(section.shake)}
        };
    }

    CinematicSection SectionFromJson(const nlohmann::json& json)
    {
        CinematicSection section;
        section.sectionId = json.value("sectionId", 0ull);
        section.trackType = static_cast<CinematicTrackType>(json.value("trackType", 0));
        section.label = json.value("label", "");
        section.startFrame = json.value("startFrame", 0);
        section.endFrame = json.value("endFrame", 60);
        section.rowIndex = json.value("rowIndex", 0);
        section.muted = json.value("muted", false);
        section.locked = json.value("locked", false);
        section.evalPolicy = static_cast<CinematicEvalPolicy>(json.value("evalPolicy", 1));
        section.seekPolicy = static_cast<CinematicSeekPolicy>(json.value("seekPolicy", 2));
        section.easeInFrames = json.value("easeInFrames", 0.0f);
        section.easeOutFrames = json.value("easeOutFrames", 0.0f);
        section.color = json.value("color", 0xFF4A90E2u);
        if (json.contains("transform")) section.transform = TransformFromJson(json["transform"]);
        if (json.contains("camera")) section.camera = CameraFromJson(json["camera"]);
        if (json.contains("animation")) section.animation = AnimationFromJson(json["animation"]);
        if (json.contains("effect")) section.effect = EffectFromJson(json["effect"]);
        if (json.contains("audio")) section.audio = AudioFromJson(json["audio"]);
        if (json.contains("eventData")) section.eventData = EventFromJson(json["eventData"]);
        if (json.contains("shake")) section.shake = CameraShakeFromJson(json["shake"]);
        return section;
    }

    nlohmann::json ToJson(const CinematicTrack& track)
    {
        nlohmann::json root;
        root["trackId"] = track.trackId;
        root["type"] = static_cast<int>(track.type);
        root["displayName"] = track.displayName;
        root["muted"] = track.muted;
        root["locked"] = track.locked;
        root["sections"] = nlohmann::json::array();
        for (const auto& section : track.sections) {
            root["sections"].push_back(ToJson(section));
        }
        return root;
    }

    CinematicTrack TrackFromJson(const nlohmann::json& json)
    {
        CinematicTrack track;
        track.trackId = json.value("trackId", 0ull);
        track.type = static_cast<CinematicTrackType>(json.value("type", 0));
        track.displayName = json.value("displayName", "");
        track.muted = json.value("muted", false);
        track.locked = json.value("locked", false);
        if (json.contains("sections") && json["sections"].is_array()) {
            for (const auto& sectionJson : json["sections"]) {
                track.sections.push_back(SectionFromJson(sectionJson));
            }
        }
        return track;
    }

    nlohmann::json ToJson(const CinematicBinding& binding)
    {
        nlohmann::json root;
        root["bindingId"] = binding.bindingId;
        root["displayName"] = binding.displayName;
        root["bindingKind"] = static_cast<int>(binding.bindingKind);
        root["targetEntity"] = static_cast<uint64_t>(binding.targetEntity);
        root["spawnPrefabPath"] = binding.spawnPrefabPath;
        root["tracks"] = nlohmann::json::array();
        for (const auto& track : binding.tracks) {
            root["tracks"].push_back(ToJson(track));
        }
        return root;
    }

    CinematicBinding BindingFromJson(const nlohmann::json& json)
    {
        CinematicBinding binding;
        binding.bindingId = json.value("bindingId", 0ull);
        binding.displayName = json.value("displayName", "");
        binding.bindingKind = static_cast<CinematicBindingKind>(json.value("bindingKind", 0));
        binding.targetEntity = json.value("targetEntity", Entity::NULL_ID);
        binding.spawnPrefabPath = json.value("spawnPrefabPath", "");
        if (json.contains("tracks") && json["tracks"].is_array()) {
            for (const auto& trackJson : json["tracks"]) {
                binding.tracks.push_back(TrackFromJson(trackJson));
            }
        }
        return binding;
    }

    nlohmann::json ToJson(const CinematicFolder& folder)
    {
        return {
            {"folderId", folder.folderId},
            {"displayName", folder.displayName},
            {"expanded", folder.expanded},
            {"childFolderIds", folder.childFolderIds},
            {"bindingIds", folder.bindingIds},
            {"masterTrackIds", folder.masterTrackIds}
        };
    }

    CinematicFolder FolderFromJson(const nlohmann::json& json)
    {
        CinematicFolder folder;
        folder.folderId = json.value("folderId", 0ull);
        folder.displayName = json.value("displayName", "");
        folder.expanded = json.value("expanded", true);
        folder.childFolderIds = json.value("childFolderIds", std::vector<uint64_t>{});
        folder.bindingIds = json.value("bindingIds", std::vector<uint64_t>{});
        folder.masterTrackIds = json.value("masterTrackIds", std::vector<uint64_t>{});
        return folder;
    }

    nlohmann::json ToJson(const CinematicViewSettings& settings)
    {
        return {
            {"timelineZoom", settings.timelineZoom},
            {"showSeconds", settings.showSeconds},
            {"showCameraPaths", settings.showCameraPaths}
        };
    }

    CinematicViewSettings ViewSettingsFromJson(const nlohmann::json& json)
    {
        CinematicViewSettings settings;
        settings.timelineZoom = json.value("timelineZoom", 1.0f);
        settings.showSeconds = json.value("showSeconds", false);
        settings.showCameraPaths = json.value("showCameraPaths", true);
        return settings;
    }
}

bool CinematicSequenceSerializer::Save(const std::string& path, const CinematicSequenceAsset& asset)
{
    std::filesystem::path filePath(path);
    if (filePath.has_parent_path()) {
        std::filesystem::create_directories(filePath.parent_path());
    }

    nlohmann::json root;
    root["schemaVersion"] = 1;
    root["name"] = asset.name;
    root["frameRate"] = asset.frameRate;
    root["durationFrames"] = asset.durationFrames;
    root["playRangeStart"] = asset.playRangeStart;
    root["playRangeEnd"] = asset.playRangeEnd;
    root["workRangeStart"] = asset.workRangeStart;
    root["workRangeEnd"] = asset.workRangeEnd;
    root["masterTracks"] = nlohmann::json::array();
    root["bindings"] = nlohmann::json::array();
    root["folders"] = nlohmann::json::array();
    root["viewSettings"] = ToJson(asset.viewSettings);

    for (const auto& track : asset.masterTracks) {
        root["masterTracks"].push_back(ToJson(track));
    }
    for (const auto& binding : asset.bindings) {
        root["bindings"].push_back(ToJson(binding));
    }
    for (const auto& folder : asset.folders) {
        root["folders"].push_back(ToJson(folder));
    }

    std::ofstream output(path, std::ios::binary);
    if (!output.is_open()) {
        return false;
    }

    output << root.dump(2);
    return true;
}

bool CinematicSequenceSerializer::Load(const std::string& path, CinematicSequenceAsset& outAsset)
{
    std::ifstream input(path, std::ios::binary);
    if (!input.is_open()) {
        return false;
    }

    nlohmann::json root;
    try {
        input >> root;
    } catch (...) {
        return false;
    }

    outAsset = {};
    outAsset.name = root.value("name", "New Sequence");
    outAsset.frameRate = root.value("frameRate", 60.0f);
    outAsset.durationFrames = root.value("durationFrames", 600);
    outAsset.playRangeStart = root.value("playRangeStart", 0);
    outAsset.playRangeEnd = root.value("playRangeEnd", outAsset.durationFrames);
    outAsset.workRangeStart = root.value("workRangeStart", 0);
    outAsset.workRangeEnd = root.value("workRangeEnd", outAsset.durationFrames);

    if (root.contains("masterTracks") && root["masterTracks"].is_array()) {
        for (const auto& trackJson : root["masterTracks"]) {
            outAsset.masterTracks.push_back(TrackFromJson(trackJson));
        }
    }
    if (root.contains("bindings") && root["bindings"].is_array()) {
        for (const auto& bindingJson : root["bindings"]) {
            outAsset.bindings.push_back(BindingFromJson(bindingJson));
        }
    }
    if (root.contains("folders") && root["folders"].is_array()) {
        for (const auto& folderJson : root["folders"]) {
            outAsset.folders.push_back(FolderFromJson(folderJson));
        }
    }
    if (root.contains("viewSettings") && root["viewSettings"].is_object()) {
        outAsset.viewSettings = ViewSettingsFromJson(root["viewSettings"]);
    }

    return true;
}
