#pragma once

#include "Entity/Entity.h"

#include <DirectXMath.h>

#include <cstdint>
#include <string>
#include <vector>

enum class CinematicBindingKind
{
    Entity,
    Spawnable,
    PreviewOnly
};

enum class CinematicTrackType
{
    Transform,
    Camera,
    Animation,
    Effect,
    Audio,
    Event,
    CameraShake,
    Bool,
    Float
};

enum class CinematicEvalPolicy
{
    Static,
    Animated,
    TriggerOnly
};

enum class CinematicSeekPolicy
{
    TriggerOnSeek,
    SkipOnSeek,
    EvaluateRangeOnSeek
};

enum class CinematicCameraMode
{
    FreeCamera,
    LookAtCamera
};

enum class CinematicRetriggerPolicy
{
    RestartIfActive,
    IgnoreIfActive,
    LayerIfAllowed
};

struct CinematicTransformSection
{
    DirectX::XMFLOAT3 startPosition = { 0.0f, 0.0f, 0.0f };
    DirectX::XMFLOAT3 endPosition = { 0.0f, 0.0f, 0.0f };
    DirectX::XMFLOAT3 startRotationEuler = { 0.0f, 0.0f, 0.0f };
    DirectX::XMFLOAT3 endRotationEuler = { 0.0f, 0.0f, 0.0f };
    DirectX::XMFLOAT3 startScale = { 1.0f, 1.0f, 1.0f };
    DirectX::XMFLOAT3 endScale = { 1.0f, 1.0f, 1.0f };
};

struct CinematicCameraSection
{
    uint64_t cameraBindingId = 0;
    CinematicCameraMode cameraMode = CinematicCameraMode::FreeCamera;
    DirectX::XMFLOAT3 startPosition = { 0.0f, 2.0f, -10.0f };
    DirectX::XMFLOAT3 endPosition = { 0.0f, 2.0f, -10.0f };
    DirectX::XMFLOAT3 startRotationEuler = { 0.0f, 0.0f, 0.0f };
    DirectX::XMFLOAT3 endRotationEuler = { 0.0f, 0.0f, 0.0f };
    DirectX::XMFLOAT3 startEye = { 0.0f, 2.0f, -10.0f };
    DirectX::XMFLOAT3 endEye = { 0.0f, 2.0f, -10.0f };
    DirectX::XMFLOAT3 startTarget = { 0.0f, 1.0f, 0.0f };
    DirectX::XMFLOAT3 endTarget = { 0.0f, 1.0f, 0.0f };
    float startFovDeg = 45.0f;
    float endFovDeg = 45.0f;
    float startRollDeg = 0.0f;
    float endRollDeg = 0.0f;
    float startFocusDistance = 500.0f;
    float endFocusDistance = 500.0f;
    float startAperture = 2.8f;
    float endAperture = 2.8f;
    float blendEaseIn = 0.0f;
    float blendEaseOut = 0.0f;
};

struct CinematicAnimationSection
{
    int animationIndex = -1;
    std::string animationName;
    bool loop = true;
    float playRate = 1.0f;
    float blendInFrames = 6.0f;
    float blendOutFrames = 6.0f;
    std::string slot = "Base";
};

struct CinematicScalarOverride
{
    std::string name;
    float value = 0.0f;
};

struct CinematicColorOverride
{
    std::string name;
    DirectX::XMFLOAT4 value = { 1.0f, 1.0f, 1.0f, 1.0f };
};

struct CinematicEffectSection
{
    std::string effectAssetPath;
    std::string socketName;
    DirectX::XMFLOAT3 offsetPosition = { 0.0f, 0.0f, 0.0f };
    DirectX::XMFLOAT3 offsetRotation = { 0.0f, 0.0f, 0.0f };
    DirectX::XMFLOAT3 offsetScale = { 1.0f, 1.0f, 1.0f };
    uint32_t seed = 1;
    bool loop = false;
    bool fireOnEnterOnly = true;
    bool stopOnExit = true;
    CinematicRetriggerPolicy retriggerPolicy = CinematicRetriggerPolicy::RestartIfActive;
    std::vector<CinematicScalarOverride> assetOverrides;
    std::vector<CinematicScalarOverride> editorPreviewScalarOverrides;
    std::vector<CinematicColorOverride> colorOverrides;
};

struct CinematicAudioSection
{
    std::string audioAssetPath;
    bool is3D = true;
    std::string attachSocket;
    float volume = 1.0f;
    float pitch = 1.0f;
    bool loop = false;
    float startOffsetSec = 0.0f;
    bool stopOnExit = true;
    CinematicRetriggerPolicy retriggerPolicy = CinematicRetriggerPolicy::RestartIfActive;
};

struct CinematicEventSection
{
    std::string eventName;
    std::string eventCategory;
    std::string payloadType = "String";
    std::string payloadJson;
    bool fireOnce = true;
};

struct CinematicCameraShakeSection
{
    float duration = 0.2f;
    float amplitude = 0.5f;
    float frequency = 20.0f;
    float decay = 0.9f;
    float hitStopDuration = 0.0f;
    float timeScale = 0.0f;
};

struct CinematicSection
{
    uint64_t sectionId = 0;
    CinematicTrackType trackType = CinematicTrackType::Transform;
    std::string label;
    int startFrame = 0;
    int endFrame = 60;
    int rowIndex = 0;
    bool muted = false;
    bool locked = false;
    CinematicEvalPolicy evalPolicy = CinematicEvalPolicy::Animated;
    CinematicSeekPolicy seekPolicy = CinematicSeekPolicy::EvaluateRangeOnSeek;
    float easeInFrames = 0.0f;
    float easeOutFrames = 0.0f;
    uint32_t color = 0xFF4A90E2;

    CinematicTransformSection transform;
    CinematicCameraSection camera;
    CinematicAnimationSection animation;
    CinematicEffectSection effect;
    CinematicAudioSection audio;
    CinematicEventSection eventData;
    CinematicCameraShakeSection shake;
};

struct CinematicTrack
{
    uint64_t trackId = 0;
    CinematicTrackType type = CinematicTrackType::Transform;
    std::string displayName;
    bool muted = false;
    bool locked = false;
    std::vector<CinematicSection> sections;
};

struct CinematicBinding
{
    uint64_t bindingId = 0;
    std::string displayName;
    CinematicBindingKind bindingKind = CinematicBindingKind::Entity;
    EntityID targetEntity = Entity::NULL_ID;
    std::string spawnPrefabPath;
    std::vector<CinematicTrack> tracks;
};

struct CinematicFolder
{
    uint64_t folderId = 0;
    std::string displayName;
    bool expanded = true;
    std::vector<uint64_t> childFolderIds;
    std::vector<uint64_t> bindingIds;
    std::vector<uint64_t> masterTrackIds;
};

struct CinematicViewSettings
{
    float timelineZoom = 1.0f;
    bool showSeconds = false;
    bool showCameraPaths = true;
};

struct CinematicSequenceAsset
{
    std::string name = "New Sequence";
    float frameRate = 60.0f;
    int durationFrames = 600;
    int playRangeStart = 0;
    int playRangeEnd = 600;
    int workRangeStart = 0;
    int workRangeEnd = 600;
    std::vector<CinematicTrack> masterTracks;
    std::vector<CinematicBinding> bindings;
    std::vector<CinematicFolder> folders;
    CinematicViewSettings viewSettings;
};

const char* GetCinematicTrackTypeLabel(CinematicTrackType type);
const char* GetCinematicCameraModeLabel(CinematicCameraMode mode);
const char* GetCinematicEvalPolicyLabel(CinematicEvalPolicy policy);
const char* GetCinematicSeekPolicyLabel(CinematicSeekPolicy policy);
const char* GetCinematicRetriggerPolicyLabel(CinematicRetriggerPolicy policy);

CinematicTrack MakeDefaultCinematicTrack(CinematicTrackType type, uint64_t trackId);
CinematicSection MakeDefaultCinematicSection(CinematicTrackType type, uint64_t sectionId, int startFrame, int endFrame);
