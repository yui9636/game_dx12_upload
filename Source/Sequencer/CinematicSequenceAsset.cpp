#include "CinematicSequenceAsset.h"

namespace
{
    static uint32_t DefaultTrackColor(CinematicTrackType type)
    {
        switch (type) {
        case CinematicTrackType::Transform: return 0xFF3FA7D6;
        case CinematicTrackType::Camera: return 0xFFFFA726;
        case CinematicTrackType::Animation: return 0xFF66BB6A;
        case CinematicTrackType::Effect: return 0xFFAB47BC;
        case CinematicTrackType::Audio: return 0xFFEF5350;
        case CinematicTrackType::Event: return 0xFFFFEE58;
        case CinematicTrackType::CameraShake: return 0xFFFF7043;
        case CinematicTrackType::Bool: return 0xFF90A4AE;
        case CinematicTrackType::Float: return 0xFF26C6DA;
        default: return 0xFF4A90E2;
        }
    }
}

const char* GetCinematicTrackTypeLabel(CinematicTrackType type)
{
    switch (type) {
    case CinematicTrackType::Transform: return "Transform";
    case CinematicTrackType::Camera: return "Camera";
    case CinematicTrackType::Animation: return "Animation";
    case CinematicTrackType::Effect: return "Effect";
    case CinematicTrackType::Audio: return "Audio";
    case CinematicTrackType::Event: return "Event";
    case CinematicTrackType::CameraShake: return "Camera Shake";
    case CinematicTrackType::Bool: return "Bool";
    case CinematicTrackType::Float: return "Float";
    default: return "Unknown";
    }
}

const char* GetCinematicCameraModeLabel(CinematicCameraMode mode)
{
    switch (mode) {
    case CinematicCameraMode::FreeCamera: return "FreeCamera";
    case CinematicCameraMode::LookAtCamera: return "LookAtCamera";
    default: return "Unknown";
    }
}

const char* GetCinematicEvalPolicyLabel(CinematicEvalPolicy policy)
{
    switch (policy) {
    case CinematicEvalPolicy::Static: return "Static";
    case CinematicEvalPolicy::Animated: return "Animated";
    case CinematicEvalPolicy::TriggerOnly: return "TriggerOnly";
    default: return "Unknown";
    }
}

const char* GetCinematicSeekPolicyLabel(CinematicSeekPolicy policy)
{
    switch (policy) {
    case CinematicSeekPolicy::TriggerOnSeek: return "TriggerOnSeek";
    case CinematicSeekPolicy::SkipOnSeek: return "SkipOnSeek";
    case CinematicSeekPolicy::EvaluateRangeOnSeek: return "EvaluateRangeOnSeek";
    default: return "Unknown";
    }
}

const char* GetCinematicRetriggerPolicyLabel(CinematicRetriggerPolicy policy)
{
    switch (policy) {
    case CinematicRetriggerPolicy::RestartIfActive: return "RestartIfActive";
    case CinematicRetriggerPolicy::IgnoreIfActive: return "IgnoreIfActive";
    case CinematicRetriggerPolicy::LayerIfAllowed: return "LayerIfAllowed";
    default: return "Unknown";
    }
}

CinematicTrack MakeDefaultCinematicTrack(CinematicTrackType type, uint64_t trackId)
{
    CinematicTrack track;
    track.trackId = trackId;
    track.type = type;
    track.displayName = GetCinematicTrackTypeLabel(type);
    return track;
}

CinematicSection MakeDefaultCinematicSection(CinematicTrackType type, uint64_t sectionId, int startFrame, int endFrame)
{
    CinematicSection section;
    section.sectionId = sectionId;
    section.trackType = type;
    section.label = GetCinematicTrackTypeLabel(type);
    section.startFrame = startFrame;
    section.endFrame = endFrame;
    section.color = DefaultTrackColor(type);
    section.evalPolicy = (type == CinematicTrackType::Event) ? CinematicEvalPolicy::TriggerOnly : CinematicEvalPolicy::Animated;

    if (type == CinematicTrackType::Camera) {
        section.label = "Camera Cut";
        section.camera.startPosition = { 0.0f, 2.0f, -10.0f };
        section.camera.endPosition = { 0.0f, 2.0f, -10.0f };
        section.camera.startTarget = { 0.0f, 1.0f, 0.0f };
        section.camera.endTarget = { 0.0f, 1.0f, 0.0f };
    } else if (type == CinematicTrackType::Effect) {
        section.effect.stopOnExit = true;
        section.effect.fireOnEnterOnly = true;
    } else if (type == CinematicTrackType::Audio) {
        section.audio.stopOnExit = true;
        section.audio.is3D = true;
    } else if (type == CinematicTrackType::Event) {
        section.eventData.eventName = "CinematicEvent";
        section.eventData.eventCategory = "Gameplay";
    }

    return section;
}
