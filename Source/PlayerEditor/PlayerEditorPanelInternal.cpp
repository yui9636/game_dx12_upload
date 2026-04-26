#include "PlayerEditorPanelInternal.h"

#include <cmath>
#include <cstdio>
#include <cstring>
#include <DirectXMath.h>

#include "Gameplay/ActionDatabaseComponent.h"
#include "Model/Model.h"

namespace PlayerEditorInternal
{
    std::string GenerateDefaultTrackName(const TimelineAsset& asset, TimelineTrackType type)
    {
        const char* baseName = "Track";
        switch (type) {
        case TimelineTrackType::Hitbox:      baseName = "Hitbox"; break;
        case TimelineTrackType::VFX:         baseName = "VFX"; break;
        case TimelineTrackType::Audio:       baseName = "Audio"; break;
        case TimelineTrackType::CameraShake: baseName = "Shake"; break;
        case TimelineTrackType::Event:       baseName = "Event"; break;
        case TimelineTrackType::Animation:   baseName = "Animation"; break;
        case TimelineTrackType::Camera:      baseName = "Camera"; break;
        default:                             baseName = "Custom"; break;
        }

        int nextIndex = 1;
        for (const auto& track : asset.tracks) {
            if (track.type == type) {
                ++nextIndex;
            }
        }
        char buffer[64];
        std::snprintf(buffer, sizeof(buffer), "%s %02d", baseName, nextIndex);
        return buffer;
    }

    TimelineItem CreateDefaultTimelineItem(TimelineTrackType type, int startFrame)
    {
        TimelineItem item;
        item.startFrame = (std::max)(0, startFrame);

        switch (type) {
        case TimelineTrackType::Hitbox:
            item.endFrame = item.startFrame + 8;
            item.hitbox.radius = 30.0f;
            break;
        case TimelineTrackType::VFX:
            item.endFrame = item.startFrame + 12;
            item.vfx.offsetScale = { 1.0f, 1.0f, 1.0f };
            break;
        case TimelineTrackType::Audio:
            item.endFrame = item.startFrame + 18;
            item.audio.volume = 1.0f;
            item.audio.pitch = 1.0f;
            break;
        case TimelineTrackType::CameraShake:
            item.endFrame = item.startFrame + 6;
            item.shake.duration = 0.15f;
            item.shake.amplitude = 0.5f;
            item.shake.frequency = 20.0f;
            break;
        case TimelineTrackType::Event:
            item.endFrame = item.startFrame;
            strcpy_s(item.eventName, "Event");
            break;
        default:
            item.endFrame = item.startFrame + 15;
            break;
        }

        return item;
    }

    AxisBinding* FindAxisBinding(InputActionMapAsset& map, const char* axisName)
    {
        for (auto& axis : map.axes) {
            if (axis.axisName == axisName) {
                return &axis;
            }
        }
        return nullptr;
    }

    ActionBinding* FindActionBinding(InputActionMapAsset& map, const char* actionName)
    {
        for (auto& action : map.actions) {
            if (action.actionName == actionName) {
                return &action;
            }
        }
        return nullptr;
    }

    bool EnsurePhase1AAxisBinding(
        InputActionMapAsset& map,
        const char* axisName,
        uint32_t positiveKey,
        uint32_t negativeKey,
        uint8_t gamepadAxis)
    {
        bool changed = false;
        AxisBinding* axis = FindAxisBinding(map, axisName);
        if (!axis) {
            AxisBinding newAxis;
            newAxis.axisName = axisName;
            map.axes.push_back(newAxis);
            axis = &map.axes.back();
            changed = true;
        }

        if (axis->positiveKey == 0)        { axis->positiveKey = positiveKey;     changed = true; }
        if (axis->negativeKey == 0)        { axis->negativeKey = negativeKey;     changed = true; }
        if (axis->gamepadAxis == 0xFF)     { axis->gamepadAxis = gamepadAxis;     changed = true; }
        if (axis->deadzone <= 0.0f)        { axis->deadzone = 0.15f;              changed = true; }
        if (axis->sensitivity == 0.0f)     { axis->sensitivity = 1.0f;            changed = true; }

        return changed;
    }

    bool EnsurePhase1BActionBinding(
        InputActionMapAsset& map,
        const char* actionName,
        uint32_t scancode,
        uint8_t mouseButton,
        uint8_t gamepadButton)
    {
        bool changed = false;
        ActionBinding* action = FindActionBinding(map, actionName);
        if (!action) {
            ActionBinding newAction;
            newAction.actionName = actionName;
            map.actions.push_back(newAction);
            action = &map.actions.back();
            changed = true;
        }

        if (action->scancode == 0)         { action->scancode = scancode;         changed = true; }
        if (action->mouseButton == 0)      { action->mouseButton = mouseButton;   changed = true; }
        if (action->gamepadButton == 0xFF) { action->gamepadButton = gamepadButton;changed = true; }
        if (action->trigger != ActionTriggerType::Pressed) {
            action->trigger = ActionTriggerType::Pressed;
            changed = true;
        }

        return changed;
    }

    bool MoveAxisBindingTo(InputActionMapAsset& map, const char* axisName, size_t targetIndex)
    {
        for (size_t i = 0; i < map.axes.size(); ++i) {
            if (map.axes[i].axisName != axisName) {
                continue;
            }
            if (i == targetIndex) {
                return false;
            }

            AxisBinding axis = map.axes[i];
            map.axes.erase(map.axes.begin() + static_cast<std::ptrdiff_t>(i));
            if (targetIndex > map.axes.size()) {
                targetIndex = map.axes.size();
            }
            map.axes.insert(map.axes.begin() + static_cast<std::ptrdiff_t>(targetIndex), axis);
            return true;
        }
        return false;
    }

    bool MoveActionBindingTo(InputActionMapAsset& map, const char* actionName, size_t targetIndex)
    {
        for (size_t i = 0; i < map.actions.size(); ++i) {
            if (map.actions[i].actionName != actionName) {
                continue;
            }
            if (i == targetIndex) {
                return false;
            }

            ActionBinding action = map.actions[i];
            map.actions.erase(map.actions.begin() + static_cast<std::ptrdiff_t>(i));
            if (targetIndex > map.actions.size()) {
                targetIndex = map.actions.size();
            }
            map.actions.insert(map.actions.begin() + static_cast<std::ptrdiff_t>(targetIndex), action);
            return true;
        }
        return false;
    }

    // Spec §9: Setup Full Player generates Move axes + Attack/Dodge actions.
    bool EnsurePlayerInputMap(InputActionMapAsset& map)
    {
        bool changed = false;
        if (map.name.empty()) {
            map.name = "PlayerDefault";
            changed = true;
        }
        if (map.contextCategory.empty()) {
            map.contextCategory = "RuntimeGameplay";
            changed = true;
        }

        changed |= EnsurePhase1AAxisBinding(map, "MoveX", kScancodeD, kScancodeA, kGamepadAxisLeftX);
        changed |= EnsurePhase1AAxisBinding(map, "MoveY", kScancodeW, kScancodeS, kGamepadAxisLeftY);
        changed |= EnsurePhase1BActionBinding(map, "Attack", kScancodeJ, kMouseButtonLeft, kGamepadButtonX);
        changed |= EnsurePhase1BActionBinding(map, "Dodge", kScancodeSpace, 0, kGamepadButtonB);
        changed |= MoveActionBindingTo(map, "Attack", 0);
        changed |= MoveActionBindingTo(map, "Dodge", 1);
        changed |= MoveAxisBindingTo(map, "MoveX", 0);
        changed |= MoveAxisBindingTo(map, "MoveY", 1);
        return changed;
    }

    // Spec §11: ActionDatabase Attack1〜3 nodes.
    bool EnsureAttackComboActionNodes(ActionDatabaseComponent& database,
        int (*resolveAnim)(int slot, void* user), void* user)
    {
        bool changed = false;
        if (database.nodeCount < 3) {
            database.nodeCount = 3;
            changed = true;
        }

        struct AttackTuning {
            float comboStart;
            float cancelStart;
            int damage;
        };
        static const AttackTuning kTuning[3] = {
            { 0.4f, 0.2f, 10 },
            { 0.4f, 0.2f, 12 },
            { 0.5f, 0.3f, 18 },
        };

        for (int slot = 0; slot < 3; ++slot) {
            ActionNode& node = database.nodes[slot];
            const int animIndex = resolveAnim ? resolveAnim(slot + 1, user) : -1;
            if (animIndex >= 0 && node.animIndex < 0) {
                node.animIndex = animIndex;
                changed = true;
            }
            if (node.nextLight != -1)         { node.nextLight = -1;                   changed = true; }
            if (node.nextHeavy != -1)         { node.nextHeavy = -1;                   changed = true; }
            if (node.inputStart != 0.0f)      { node.inputStart = 0.0f;                changed = true; }
            if (node.inputEnd != 1.0f)        { node.inputEnd = 1.0f;                  changed = true; }
            if (node.comboStart != kTuning[slot].comboStart) {
                node.comboStart = kTuning[slot].comboStart;                            changed = true;
            }
            if (node.cancelStart != kTuning[slot].cancelStart) {
                node.cancelStart = kTuning[slot].cancelStart;                          changed = true;
            }
            if (node.damageVal <= 0)          { node.damageVal = kTuning[slot].damage; changed = true; }
            if (node.animSpeed <= 0.0f)       { node.animSpeed = 1.0f;                 changed = true; }
        }
        return changed;
    }

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
        ImVec2& outScreenPos)
    {
        if (!model) {
            return false;
        }

        const auto& nodes = model->GetNodes();
        if (boneIndex < 0 || boneIndex >= static_cast<int>(nodes.size())) {
            return false;
        }

        const auto& node = nodes[boneIndex];
        const float scale = (std::max)(previewScale, 0.01f);

        using namespace DirectX;
        const XMVECTOR localPos = XMVectorSet(
            node.worldTransform._41 * scale,
            node.worldTransform._42 * scale,
            node.worldTransform._43 * scale,
            1.0f);

        const float aspect = imageSize.y > 0.0f ? (imageSize.x / imageSize.y) : 1.0f;
        const float safeAspect = aspect > 0.01f ? aspect : 1.0f;
        const float safeNearZ = nearZ > 0.0001f ? nearZ : 0.03f;
        const float safeFarZ = farZ > safeNearZ ? farZ : (safeNearZ + 500.0f);
        const float safeFovY = fovY > 0.01f ? fovY : 0.785398f;

        const XMVECTOR eye = XMLoadFloat3(&cameraPosition);
        const XMVECTOR at  = XMLoadFloat3(&cameraTarget);
        const XMVECTOR up  = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
        const XMMATRIX view = XMMatrixLookAtLH(eye, at, up);
        const XMMATRIX proj = XMMatrixPerspectiveFovLH(safeFovY, safeAspect, safeNearZ, safeFarZ);
        const XMVECTOR clip = XMVector4Transform(localPos, view * proj);

        const float clipW = XMVectorGetW(clip);
        if (clipW <= 0.0001f) {
            return false;
        }

        const float ndcX = XMVectorGetX(clip) / clipW;
        const float ndcY = XMVectorGetY(clip) / clipW;
        const float ndcZ = XMVectorGetZ(clip) / clipW;
        if (ndcZ < 0.0f || ndcZ > 1.0f) {
            return false;
        }

        outScreenPos.x = imageMin.x + (ndcX * 0.5f + 0.5f) * imageSize.x;
        outScreenPos.y = imageMin.y + (-ndcY * 0.5f + 0.5f) * imageSize.y;
        return true;
    }

    bool DrawDetachedTopTabBar(bool* p_open)
    {
        ImGuiStyle& style = ImGui::GetStyle();

        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8.0f, 4.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4.0f, 0.0f));
        ImGui::PushStyleColor(ImGuiCol_ChildBg, style.Colors[ImGuiCol_MenuBarBg]);

        const bool childOpen = ImGui::BeginChild(
            "##PlayerEditorDetachedTopTabs",
            ImVec2(0.0f, kDetachedTopTabHeight),
            false,
            ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

        ImGui::PopStyleColor();
        ImGui::PopStyleVar(2);

        if (!childOpen) {
            ImGui::EndChild();
            return true;
        }

        const ImVec2 min = ImGui::GetWindowPos();
        const ImVec2 max = ImVec2(min.x + ImGui::GetWindowWidth(), min.y + ImGui::GetWindowHeight());
        ImGui::GetWindowDrawList()->AddLine(
            ImVec2(min.x, max.y - 1.0f),
            ImVec2(max.x, max.y - 1.0f),
            ImGui::GetColorU32(ImGuiCol_Border));

        bool tabOpen = p_open ? *p_open : true;

        ImGui::PushStyleVar(ImGuiStyleVar_TabRounding, 4.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(12.0f, 8.0f));

        if (ImGui::BeginTabBar(
            "##PlayerEditorDetachedDocumentTabs",
            ImGuiTabBarFlags_FittingPolicyScroll | ImGuiTabBarFlags_NoTooltip))
        {
            if (ImGui::BeginTabItem(
                ICON_FA_USER " Player Editor",
                p_open ? &tabOpen : nullptr,
                ImGuiTabItemFlags_NoReorder | ImGuiTabItemFlags_NoCloseWithMiddleMouseButton))
            {
                ImGui::EndTabItem();
            }
            ImGui::EndTabBar();
        }

        ImGui::PopStyleVar(2);
        ImGui::EndChild();

        if (p_open && !tabOpen) {
            *p_open = false;
            return false;
        }

        return true;
    }
}
