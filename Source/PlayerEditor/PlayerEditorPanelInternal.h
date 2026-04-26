#pragma once

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <filesystem>
#include <initializer_list>
#include <string>

#include <imgui.h>
#include "Icon/IconsFontAwesome7.h"

#include "Component/ColliderComponent.h"
#include "Input/InputActionMapAsset.h"
#include "StateMachineAsset.h"
#include "TimelineAsset.h"

struct ActionDatabaseComponent;
class Model;

// Internal helpers shared between PlayerEditorPanel.cpp and its extracted
// sibling modules (PlayerEditorViewportPanel.cpp / PlayerEditorSkeletonPanel.cpp /
// PlayerEditorStateMachinePanel.cpp / PlayerEditorTimelinePanel.cpp /
// PlayerEditorInspectorPanel.cpp).
namespace PlayerEditorInternal
{
    // ---- Layout / window-title constants ----
    inline constexpr float kTrackHeaderWidth     = 160.0f;
    inline constexpr float kTrackHeight          = 26.0f;
    inline constexpr float kMinPixelsPerFrame    = 2.0f;
    inline constexpr float kDefaultPPF           = 5.0f;
    inline constexpr float kPlayheadTriSize      = 8.0f;
    inline constexpr float kDetachedTopTabHeight = 34.0f;

    inline constexpr float kNodeWidth  = 150.0f;
    inline constexpr float kNodeHeight = 38.0f;

    inline constexpr const char* kModelFileFilter =
        "Player Source (*.prefab;*.fbx;*.gltf;*.glb;*.obj)\0*.prefab;*.fbx;*.gltf;*.glb;*.obj\0Prefab (*.prefab)\0*.prefab\0Model Files (*.fbx;*.gltf;*.glb;*.obj)\0*.fbx;*.gltf;*.glb;*.obj\0All Files (*.*)\0*.*\0";
    inline constexpr const char* kAudioFileFilter =
        "Audio Files (*.wav;*.ogg;*.mp3;*.flac)\0*.wav;*.ogg;*.mp3;*.flac\0WAV (*.wav)\0*.wav\0OGG (*.ogg)\0*.ogg\0MP3 (*.mp3)\0*.mp3\0FLAC (*.flac)\0*.flac\0All Files (*.*)\0*.*\0";

    inline constexpr uint32_t kScancodeA      = 4;
    inline constexpr uint32_t kScancodeD      = 7;
    inline constexpr uint32_t kScancodeS      = 22;
    inline constexpr uint32_t kScancodeW      = 26;
    inline constexpr uint32_t kScancodeJ      = 13;
    inline constexpr uint32_t kScancodeSpace  = 44;
    inline constexpr uint8_t  kMouseButtonLeft   = 1;
    inline constexpr uint8_t  kGamepadButtonX    = 2;
    inline constexpr uint8_t  kGamepadButtonB    = 1;
    inline constexpr uint8_t  kGamepadAxisLeftX  = 0;
    inline constexpr uint8_t  kGamepadAxisLeftY  = 1;

    inline constexpr const char* kPEViewportTitle     = ICON_FA_CUBE              " Viewport##PE";
    inline constexpr const char* kPESkeletonTitle     = ICON_FA_BONE              " Skeleton##PE";
    inline constexpr const char* kPEStateMachineTitle = ICON_FA_DIAGRAM_PROJECT   " State Machine##PE";
    inline constexpr const char* kPETimelineTitle     = ICON_FA_TIMELINE          " Timeline##PE";
    inline constexpr const char* kPEPropertiesTitle   = ICON_FA_SLIDERS           " Properties##PE";
    inline constexpr const char* kPEAnimatorTitle     = ICON_FA_PERSON_RUNNING    " Animator##PE";
    inline constexpr const char* kPEInputTitle        = ICON_FA_GAMEPAD           " Input##PE";

    inline const char* const kTrackTypeNames[] = {
        "Animation", "Hitbox", "VFX", "Audio", "CameraShake", "Camera", "Event", "Custom"
    };

    // ---- String helpers ----
    inline std::string ToLowerAscii(std::string value)
    {
        std::transform(value.begin(), value.end(), value.begin(),
            [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        return value;
    }

    inline bool EqualsIgnoreCase(const std::string& value, const char* rhs)
    {
        if (!rhs) {
            return false;
        }
        return ToLowerAscii(value) == ToLowerAscii(rhs);
    }

    // ---- Path / file helpers ----
    inline std::string MakeDataRelativePath(const std::string& path)
    {
        if (path.empty()) {
            return {};
        }

        std::string normalized = path;
        std::replace(normalized.begin(), normalized.end(), '\\', '/');
        const size_t dataPos = normalized.find("Data/");
        if (dataPos != std::string::npos) {
            return normalized.substr(dataPos);
        }
        return normalized;
    }

    inline bool HasExtension(const std::string& path, std::initializer_list<const char*> extensions)
    {
        std::string ext = std::filesystem::path(path).extension().string();
        std::transform(ext.begin(), ext.end(), ext.begin(),
            [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        for (const char* candidate : extensions) {
            if (ext == candidate) {
                return true;
            }
        }
        return false;
    }

    // ---- Asset content checks ----
    inline bool HasTimelineAssetContent(const TimelineAsset& asset)
    {
        return asset.id != 0
            || !asset.name.empty()
            || !asset.tracks.empty();
    }

    // ---- Enum-to-label resolvers ----
    inline const char* GetColliderAttributeLabel(ColliderAttribute attribute)
    {
        switch (attribute) {
        case ColliderAttribute::Attack:
            return "Attack";
        case ColliderAttribute::Body:
        default:
            return "Body";
        }
    }

    inline const char* GetColliderShapeLabel(ColliderShape shape)
    {
        switch (shape) {
        case ColliderShape::Capsule: return "Capsule";
        case ColliderShape::Box:     return "Box";
        case ColliderShape::Sphere:
        default:                     return "Sphere";
        }
    }

    inline const char* GetStateTypeLabel(StateNodeType type)
    {
        switch (type) {
        case StateNodeType::Locomotion: return "Locomotion";
        case StateNodeType::Action:     return "Action";
        case StateNodeType::Dodge:      return "Dodge";
        case StateNodeType::Jump:       return "Jump";
        case StateNodeType::Damage:     return "Damage";
        case StateNodeType::Dead:       return "Dead";
        default:                        return "Custom";
        }
    }

    inline const char* ResolveConditionTypeLabel(ConditionType type)
    {
        switch (type) {
        case ConditionType::Input:     return "Input";
        case ConditionType::Timer:     return "Timer";
        case ConditionType::AnimEnd:   return "AnimEnd";
        case ConditionType::Health:    return "Health";
        case ConditionType::Stamina:   return "Stamina";
        default:                       return "Parameter";
        }
    }

    inline const char* ResolveCompareOpLabel(CompareOp compare)
    {
        switch (compare) {
        case CompareOp::Equal:        return "==";
        case CompareOp::NotEqual:     return "!=";
        case CompareOp::Greater:      return ">";
        case CompareOp::Less:         return "<";
        case CompareOp::GreaterEqual: return ">=";
        default:                      return "<=";
        }
    }

    inline ImU32 StateNodeColor(StateNodeType type)
    {
        switch (type) {
        case StateNodeType::Locomotion: return IM_COL32(50, 120, 200, 255);
        case StateNodeType::Action:     return IM_COL32(200, 55, 55, 255);
        case StateNodeType::Dodge:      return IM_COL32(50, 170, 70, 255);
        case StateNodeType::Jump:       return IM_COL32(200, 200, 50, 255);
        case StateNodeType::Damage:     return IM_COL32(200, 110, 30, 255);
        case StateNodeType::Dead:       return IM_COL32(90, 90, 90, 255);
        default:                        return IM_COL32(140, 140, 140, 255);
        }
    }

    // ---- Timeline helpers ----
    std::string GenerateDefaultTrackName(const TimelineAsset& asset, TimelineTrackType type);
    TimelineItem CreateDefaultTimelineItem(TimelineTrackType type, int startFrame);

    // ---- Input map helpers (used by ApplyFullPlayerPreset / EnsurePlayerInputMap) ----
    AxisBinding*   FindAxisBinding(InputActionMapAsset& map, const char* axisName);
    ActionBinding* FindActionBinding(InputActionMapAsset& map, const char* actionName);
    bool EnsurePhase1AAxisBinding(InputActionMapAsset& map, const char* axisName,
        uint32_t positiveKey, uint32_t negativeKey, uint8_t gamepadAxis);
    bool EnsurePhase1BActionBinding(InputActionMapAsset& map, const char* actionName,
        uint32_t scancode, uint8_t mouseButton, uint8_t gamepadButton);
    bool MoveAxisBindingTo(InputActionMapAsset& map, const char* axisName, size_t targetIndex);
    bool MoveActionBindingTo(InputActionMapAsset& map, const char* actionName, size_t targetIndex);
    bool EnsurePlayerInputMap(InputActionMapAsset& map);

    // ---- ActionDatabase helpers ----
    bool EnsureAttackComboActionNodes(ActionDatabaseComponent& database,
        int (*resolveAnim)(int slot, void* user), void* user);

    // ---- Viewport helpers ----
    bool ProjectBoneMarkerToViewport(
        const Model* model,
        int boneIndex,
        float previewScale,
        const DirectX::XMFLOAT3& cameraPosition,
        const DirectX::XMFLOAT3& cameraTarget,
        float fovY,
        float nearZ,
        float farZ,
        const ImVec2& imageMin,
        const ImVec2& imageSize,
        ImVec2& outScreenPos);

    bool DrawDetachedTopTabBar(bool* p_open);
}
