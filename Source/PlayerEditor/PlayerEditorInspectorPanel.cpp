// ============================================================================
// PlayerEditor — Properties / Animator / Input panels (right column).
// Sibling of PlayerEditorPanel.cpp; split out for readability.
// ============================================================================
#include "PlayerEditorPanel.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <cstdint>
#include <cfloat>
#include <cstring>
#include <string>

#include <imgui.h>

#include "Icon/IconsFontAwesome7.h"
#include "PlayerEditorPanelInternal.h"
#include "PlayerEditorSession.h"
#include "Model/Model.h"
#include "Registry/Registry.h"
#include "System/Dialog.h"

using namespace PlayerEditorInternal;

void PlayerEditorPanel::DrawPropertiesPanel()
{
    if (!ImGui::Begin(kPEPropertiesTitle)) { ImGui::End(); return; }

    switch (m_selectionCtx) {
    case SelectionContext::StateNode:
        DrawStateNodeInspector();
        break;
    case SelectionContext::Transition:
    {
        StateTransition* trans = nullptr;
        for (auto& t : m_stateMachineAsset.transitions)
            if (t.id == m_selectedTransitionId) { trans = &t; break; }
        if (trans) DrawTransitionConditionEditor(trans);
        else ImGui::TextDisabled("Transition not found");
        break;
    }
    case SelectionContext::TimelineTrack:
    {
        ImGui::Text(ICON_FA_LAYER_GROUP " Track Properties");
        ImGui::Separator();
        for (auto& track : m_timelineAsset.tracks) {
            if ((int)track.id != m_selectedTrackId) continue;
            char nameBuf[64];
            strncpy_s(nameBuf, track.name.c_str(), _TRUNCATE);
            if (ImGui::InputText("Name", nameBuf, sizeof(nameBuf))) { track.name = nameBuf; m_timelineDirty = true; }

            int typeInt = static_cast<int>(track.type);
            if (ImGui::Combo("Type", &typeInt, kTrackTypeNames, 8)) {
                track.type = static_cast<TimelineTrackType>(typeInt);
                m_timelineDirty = true;
            }

            if (ImGui::Checkbox("Muted", &track.muted)) m_timelineDirty = true;
            if (ImGui::Checkbox("Locked", &track.locked)) m_timelineDirty = true;

            ImU32 col = track.color;
            float rgba[4] = {
                ((col >> 0) & 0xFF) / 255.0f, ((col >> 8) & 0xFF) / 255.0f,
                ((col >> 16) & 0xFF) / 255.0f, ((col >> 24) & 0xFF) / 255.0f };
            if (ImGui::ColorEdit4("Color", rgba, ImGuiColorEditFlags_NoInputs)) {
                track.color = IM_COL32(
                    (int)(rgba[0] * 255), (int)(rgba[1] * 255),
                    (int)(rgba[2] * 255), (int)(rgba[3] * 255));
                m_timelineDirty = true;
            }
            break;
        }
        break;
    }
    case SelectionContext::TimelineItem:
        DrawTimelineItemInspector();
        break;
    case SelectionContext::Bone:
    {
        ImGui::Text(ICON_FA_BONE " Bone Properties");
        ImGui::Separator();
        if (m_model && m_selectedBoneIndex >= 0 && m_selectedBoneIndex < (int)m_model->GetNodes().size()) {
            const auto& node = m_model->GetNodes()[m_selectedBoneIndex];
            ImGui::Text("Name: %s", node.name.c_str());
            ImGui::Text("Index: %d", m_selectedBoneIndex);
            ImGui::Text("Parent: %d", node.parentIndex);
            ImGui::Text("Children: %d", (int)node.children.size());
            ImGui::Separator();
            ImGui::Text("Local Transform:");
            ImGui::Text("  Pos: (%.3f, %.3f, %.3f)", node.position.x, node.position.y, node.position.z);
            ImGui::Text("  Rot: (%.3f, %.3f, %.3f, %.3f)", node.rotation.x, node.rotation.y, node.rotation.z, node.rotation.w);
            ImGui::Text("  Scale: (%.3f, %.3f, %.3f)", node.scale.x, node.scale.y, node.scale.z);
            ImGui::Separator();
            DrawPersistentColliderSection();
        }
        break;
    }
    case SelectionContext::Socket:
    {
        ImGui::Text(ICON_FA_PLUG " Socket Properties");
        ImGui::Separator();
        if (m_selectedSocketIdx >= 0 && m_selectedSocketIdx < (int)m_sockets.size()) {
            auto& sock = m_sockets[m_selectedSocketIdx];
            char nameBuf[128];
            strncpy_s(nameBuf, sock.name.c_str(), _TRUNCATE);
            if (ImGui::InputText("Name", nameBuf, sizeof(nameBuf))) { sock.name = nameBuf; m_socketDirty = true; }

            ImGui::Text("Parent Bone: %s [%d]", sock.parentBoneName.c_str(), sock.cachedBoneIndex);
            if (m_selectedBoneIndex >= 0 && !m_selectedBoneName.empty()) {
                ImGui::SameLine();
                if (ImGui::SmallButton("Use Selected")) {
                    sock.parentBoneName = m_selectedBoneName;
                    sock.cachedBoneIndex = m_selectedBoneIndex;
                    m_socketDirty = true;
                }
            }

            if (ImGui::DragFloat3("Offset Pos", &sock.offsetPos.x, 0.01f)) m_socketDirty = true;
            if (ImGui::DragFloat3("Offset Rot", &sock.offsetRotDeg.x, 0.1f)) m_socketDirty = true;
            if (ImGui::DragFloat3("Offset Scale", &sock.offsetScale.x, 0.01f, 0.01f, 10.0f)) m_socketDirty = true;

            ImGui::Separator();
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.6f, 0.15f, 0.15f, 1.0f));
            if (ImGui::Button(ICON_FA_TRASH " Delete Socket")) {
                m_sockets.erase(m_sockets.begin() + m_selectedSocketIdx);
                m_selectedSocketIdx = -1;
                m_selectionCtx = SelectionContext::None;
                m_socketDirty = true;
            }
            ImGui::PopStyleColor();
        }
        break;
    }
    case SelectionContext::PersistentCollider:
        DrawPersistentColliderInspector();
        break;
    default:
        if (!m_stateMachineAsset.states.empty() || !m_stateMachineAsset.parameters.empty()) {
            DrawStateMachineParameterList();
        } else {
            ImGui::TextDisabled("Select a state, transition, track,");
            ImGui::TextDisabled("item, bone, or socket to view");
            ImGui::TextDisabled("its properties here.");
        }
        if (HasOpenModel()) {
            ImGui::Separator();
            DrawPersistentColliderSection();
        }
        break;
    }

    ImGui::End();
}

void PlayerEditorPanel::DrawTimelineItemInspector()
{
    bool timelineRuntimeChanged = false;

    for (auto& track : m_timelineAsset.tracks) {
        if ((int)track.id != m_selectedTrackId) continue;
        if (m_selectedItemIdx < 0 || m_selectedItemIdx >= (int)track.items.size()) break;

        auto& item = track.items[m_selectedItemIdx];

        ImGui::Text("Track: %s", track.name.c_str());
        ImGui::TextDisabled("Resize the bar directly on the timeline.");

        ImGui::Separator();

        switch (track.type) {
        case TimelineTrackType::Hitbox:
            ImGui::Text(ICON_FA_CROSSHAIRS " Hitbox Payload");
            ImGui::TextDisabled("Target Bone: %s", GetBoneNameByIndex(item.hitbox.nodeIndex));
            ImGui::TextDisabled("Click a skeleton node to assign.");
            if (m_selectedSocketIdx >= 0 && m_selectedSocketIdx < static_cast<int>(m_sockets.size())) {
                const NodeSocket& socket = m_sockets[m_selectedSocketIdx];
                if (ImGui::Button("Use Selected Socket")) {
                    item.hitbox.nodeIndex = socket.cachedBoneIndex;
                    item.hitbox.offsetLocal = socket.offsetPos;
                    m_timelineDirty = true;
                    timelineRuntimeChanged = true;
                }
                ImGui::SameLine();
                ImGui::TextDisabled("%s", socket.name.c_str());
            }
            if (ImGui::DragFloat3("Offset", &item.hitbox.offsetLocal.x, 0.01f)) { m_timelineDirty = true; timelineRuntimeChanged = true; }
            if (ImGui::DragFloat("Radius", &item.hitbox.radius, 0.1f, 0.0f, 100.0f)) { m_timelineDirty = true; timelineRuntimeChanged = true; }
            {
                float rgba[4] = {
                    ((item.hitbox.rgba >> 24) & 0xFF) / 255.0f,
                    ((item.hitbox.rgba >> 16) & 0xFF) / 255.0f,
                    ((item.hitbox.rgba >> 8) & 0xFF) / 255.0f,
                    (item.hitbox.rgba & 0xFF) / 255.0f
                };
                if (ImGui::ColorEdit4("Color", rgba)) {
                    item.hitbox.rgba = ((uint32_t)(rgba[0] * 255) << 24) |
                        ((uint32_t)(rgba[1] * 255) << 16) |
                        ((uint32_t)(rgba[2] * 255) << 8) |
                        (uint32_t)(rgba[3] * 255);
                    m_timelineDirty = true;
                    timelineRuntimeChanged = true;
                }
            }
            break;

        case TimelineTrackType::VFX:
            ImGui::Text(ICON_FA_WAND_MAGIC_SPARKLES " VFX Payload");
            if (ImGui::InputText("Asset ID", item.vfx.assetId, sizeof(item.vfx.assetId))) { m_timelineDirty = true; timelineRuntimeChanged = true; }
            ImGui::TextDisabled("Target Bone: %s", GetBoneNameByIndex(item.vfx.nodeIndex));
            ImGui::TextDisabled("Click a skeleton node to assign.");
            if (m_selectedSocketIdx >= 0 && m_selectedSocketIdx < static_cast<int>(m_sockets.size())) {
                const NodeSocket& socket = m_sockets[m_selectedSocketIdx];
                if (ImGui::Button("Use Selected Socket##VFX")) {
                    item.vfx.nodeIndex = socket.cachedBoneIndex;
                    item.vfx.offsetLocal = socket.offsetPos;
                    item.vfx.offsetRotDeg = socket.offsetRotDeg;
                    item.vfx.offsetScale = socket.offsetScale;
                    m_timelineDirty = true;
                    timelineRuntimeChanged = true;
                }
                ImGui::SameLine();
                ImGui::TextDisabled("%s", socket.name.c_str());
            }
            if (ImGui::DragFloat3("Offset", &item.vfx.offsetLocal.x, 0.01f)) { m_timelineDirty = true; timelineRuntimeChanged = true; }
            if (ImGui::DragFloat3("Rotation", &item.vfx.offsetRotDeg.x, 0.1f)) { m_timelineDirty = true; timelineRuntimeChanged = true; }
            if (ImGui::DragFloat3("Scale", &item.vfx.offsetScale.x, 0.01f, 0.01f, 10.0f)) { m_timelineDirty = true; timelineRuntimeChanged = true; }
            break;

        case TimelineTrackType::Audio:
            ImGui::Text(ICON_FA_VOLUME_HIGH " Audio Payload");
            if (ImGui::InputText("Audio Path", item.audio.assetId, sizeof(item.audio.assetId))) { m_timelineDirty = true; timelineRuntimeChanged = true; }
            ImGui::SameLine();
            if (ImGui::Button(ICON_FA_FOLDER_OPEN " Browse##Audio")) {
                char pathBuffer[MAX_PATH] = {};
                if (item.audio.assetId[0] != '\0') {
                    strcpy_s(pathBuffer, item.audio.assetId);
                }
                if (Dialog::OpenFileName(pathBuffer, MAX_PATH, kAudioFileFilter, "Select Audio Clip") == DialogResult::OK) {
                    const std::string relativePath = MakeDataRelativePath(pathBuffer);
                    strcpy_s(item.audio.assetId, relativePath.c_str());
                    m_timelineDirty = true;
                    timelineRuntimeChanged = true;
                }
            }
            if (ImGui::DragFloat("Volume", &item.audio.volume, 0.01f, 0.0f, 1.0f)) { m_timelineDirty = true; timelineRuntimeChanged = true; }
            if (ImGui::DragFloat("Pitch", &item.audio.pitch, 0.01f, 0.1f, 3.0f)) { m_timelineDirty = true; timelineRuntimeChanged = true; }
            if (ImGui::Checkbox("3D Audio", &item.audio.is3D)) { m_timelineDirty = true; timelineRuntimeChanged = true; }
            if (ImGui::Checkbox("Loop", &item.audio.loop)) { m_timelineDirty = true; timelineRuntimeChanged = true; }
            break;

        case TimelineTrackType::CameraShake:
            ImGui::Text(ICON_FA_CAMERA " Camera Shake Payload");
            if (ImGui::DragFloat("Duration", &item.shake.duration, 0.01f, 0.0f, 5.0f)) { m_timelineDirty = true; timelineRuntimeChanged = true; }
            if (ImGui::DragFloat("Amplitude", &item.shake.amplitude, 0.01f, 0.0f, 10.0f)) { m_timelineDirty = true; timelineRuntimeChanged = true; }
            if (ImGui::DragFloat("Frequency", &item.shake.frequency, 0.1f, 0.0f, 100.0f)) { m_timelineDirty = true; timelineRuntimeChanged = true; }
            if (ImGui::DragFloat("Decay", &item.shake.decay, 0.01f, 0.0f, 10.0f)) { m_timelineDirty = true; timelineRuntimeChanged = true; }
            if (ImGui::DragFloat("Hit Stop", &item.shake.hitStopDuration, 0.001f, 0.0f, 1.0f)) { m_timelineDirty = true; timelineRuntimeChanged = true; }
            if (ImGui::DragFloat("Time Scale", &item.shake.timeScale, 0.01f, 0.0f, 1.0f)) { m_timelineDirty = true; timelineRuntimeChanged = true; }
            break;

        case TimelineTrackType::Event:
            ImGui::Text(ICON_FA_BOLT " Event Payload");
            if (ImGui::InputText("Event Name", item.eventName, sizeof(item.eventName))) m_timelineDirty = true;
            if (ImGui::InputTextMultiline("Event Data", item.eventData, sizeof(item.eventData), ImVec2(-FLT_MIN, 80.0f))) m_timelineDirty = true;
            break;

        default:
            ImGui::TextDisabled("No payload editor for this track type.");
            break;
        }

        break;
    }

    if (timelineRuntimeChanged) {
        RebuildPreviewTimelineRuntimeData();
        SyncPreviewTimelinePlayback();
    }
}

void PlayerEditorPanel::DrawAnimatorPanel()
{
    if (!ImGui::Begin(kPEAnimatorTitle)) { ImGui::End(); return; }

    const bool hasPreviewTarget = m_registry && !Entity::IsNull(m_previewEntity) && m_registry->IsAlive(m_previewEntity);

    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(3.0f, 1.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(2.0f, 1.0f));

    if (!m_model) {
        ImGui::TextDisabled("No model assigned.");
    } else {
        const auto& animations = m_model->GetAnimations();
        if (animations.empty()) {
            ImGui::TextDisabled("Model has no animations.");
        } else {
            for (int i = 0; i < static_cast<int>(animations.size()); ++i) {
                const bool selected = (m_selectedAnimIndex == i);
                std::string label = "[" + std::to_string(i) + "] " + animations[i].name;
                if (ImGui::Selectable(label.c_str(), selected)) {
                    if (m_selectedAnimIndex != i) {
                        m_selectedAnimIndex = i;
                        PlayerEditorSession::SyncTimelineAssetSelection(*this);
                    }
                }
                if (ImGui::IsItemHovered()) {
                    ImGui::SetTooltip("Length: %.3fs", animations[i].secondsLength);
                    if (ImGui::IsMouseDoubleClicked(0) && hasPreviewTarget) {
                        m_selectedAnimIndex = i;
                        StartSelectedAnimationPreview();
                    }
                }
            }
        }
    }

    ImGui::PopStyleVar(2);
    ImGui::End();
}

void PlayerEditorPanel::DrawInputPanel()
{
    if (!ImGui::Begin(kPEInputTitle)) { ImGui::End(); return; }
    m_inputMappingTab.Draw(m_registry);
    ImGui::End();
}
