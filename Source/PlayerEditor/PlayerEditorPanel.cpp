// ============================================================================
// PlayerEditor — core panel: lifecycle, top-level Draw / DockSpace, toolbar,
// preview-entity / model setters, prefab save delegation. Concern-specific
// drawing lives in sibling files:
//   PlayerEditorViewportPanel.cpp     — 3D viewport + camera helpers
//   PlayerEditorSkeletonPanel.cpp     — bone tree, sockets, persistent colliders
//   PlayerEditorStateMachinePanel.cpp — state-machine UI + presets
//   PlayerEditorTimelinePanel.cpp     — timeline UI + playback
//   PlayerEditorInspectorPanel.cpp    — properties / animator / input panels
// Shared statics live in PlayerEditorPanelInternal.{h,cpp}.
// ============================================================================
#include "PlayerEditorPanel.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <imgui.h>
#include <imgui_internal.h>

#include "Icon/IconsFontAwesome7.h"
#include "PlayerEditorPanelInternal.h"
#include "PlayerEditorSession.h"
#include "Component/ColliderComponent.h"
#include "Gameplay/PlaybackComponent.h"
#include "Gameplay/TimelineComponent.h"
#include "Registry/Registry.h"
#include "System/Dialog.h"

using namespace PlayerEditorInternal;

// ============================================================================
// Lifecycle / preview-entity / external selection (delegated to session helpers)
// ============================================================================

void PlayerEditorPanel::Suspend()
{
    PlayerEditorSession::Suspend(*this);
}

void PlayerEditorPanel::DestroyOwnedPreviewEntity()
{
    PlayerEditorSession::DestroyOwnedPreviewEntity(*this);
}

void PlayerEditorPanel::SetSharedSceneCamera(const DirectX::XMFLOAT3& position, const DirectX::XMFLOAT3& direction, float fovY)
{
    m_sharedSceneCameraPosition = position;
    m_sharedSceneCameraDirection = direction;
    m_sharedSceneCameraFovY = fovY;
}

bool PlayerEditorPanel::ConsumePendingCameraFit(
    DirectX::XMFLOAT3& outTarget,
    float& outRadius,
    DirectX::XMFLOAT3* outForward,
    float* outDistance)
{
    if (!m_hasPendingCameraFit) {
        return false;
    }

    outTarget = m_pendingCameraFitTarget;
    outRadius = m_pendingCameraFitRadius;
    if (outForward) {
        *outForward = m_pendingCameraFitForward;
    }
    if (outDistance) {
        *outDistance = m_pendingCameraFitDistance;
    }
    m_hasPendingCameraFit = false;
    return true;
}

void PlayerEditorPanel::EnsureOwnedPreviewEntity()
{
    PlayerEditorSession::EnsureOwnedPreviewEntity(*this);
}

void PlayerEditorPanel::SetPreviewEntity(EntityID entity)
{
    PlayerEditorSession::SetPreviewEntity(*this, entity);
}

void PlayerEditorPanel::SyncExternalSelection(EntityID entity, const std::string& modelPath)
{
    PlayerEditorSession::SyncExternalSelection(*this, entity, modelPath);
}

bool PlayerEditorPanel::DrawToolbarButton(const char* label, bool enabled)
{
    if (!enabled) {
        ImGui::BeginDisabled();
    }
    const bool pressed = ImGui::Button(label);
    if (!enabled) {
        ImGui::EndDisabled();
    }
    return enabled && pressed;
}

void PlayerEditorPanel::ResetSelectionState()
{
    m_selectionCtx = SelectionContext::None;
    m_selectedTrackId = -1;
    m_selectedItemIdx = -1;
    m_selectedNodeId = 0;
    m_selectedTransitionId = 0;
    m_selectedBoneIndex = -1;
    m_hoveredBoneIndex = -1;
    m_selectedBoneName.clear();
    m_selectedSocketIdx = -1;
    m_selectedColliderIdx = -1;
    m_playheadFrame = 0;
    m_isPlaying = false;
}

bool PlayerEditorPanel::HasOpenModel() const
{
    return m_model != nullptr;
}

bool PlayerEditorPanel::HasAnyDirtyDocument() const
{
    return m_timelineDirty || m_stateMachineDirty || m_socketDirty || m_colliderDirty || m_inputMappingTab.IsDirty();
}

bool PlayerEditorPanel::HasSelectedEntityContext() const
{
    return !Entity::IsNull(m_selectedEntity) && !m_selectedEntityModelPath.empty();
}

bool PlayerEditorPanel::CanUsePreviewEntity() const
{
    return m_registry && !Entity::IsNull(m_previewEntity) && m_registry->IsAlive(m_previewEntity);
}

bool PlayerEditorPanel::OpenModelFromPath(const std::string& path)
{
    return PlayerEditorSession::OpenModelFromPath(*this, path);
}

void PlayerEditorPanel::ApplyEditorBindingsToPreviewEntity()
{
    PlayerEditorSession::ApplyEditorBindingsToPreviewEntity(*this);
}

void PlayerEditorPanel::RebuildPreviewTimelineRuntimeData()
{
    PlayerEditorSession::RebuildPreviewTimelineRuntimeData(*this);
}

void PlayerEditorPanel::SyncPreviewTimelinePlayback(bool syncPreviewState)
{
    if (m_registry && !Entity::IsNull(m_previewEntity) && m_registry->IsAlive(m_previewEntity)) {
        float fps = m_timelineAsset.fps > 0.0f ? m_timelineAsset.fps : 60.0f;
        const float clipLength = GetTimelinePlaybackDurationSeconds();
        int frameMin = 0;
        int frameMax = GetTimelineFrameLimit();

        if (TimelineComponent* timeline = m_registry->GetComponent<TimelineComponent>(m_previewEntity)) {
            if (timeline->fps > 0.0f) {
                fps = timeline->fps;
            }
            frameMin = timeline->frameMin;
        }

        if (frameMax > frameMin) {
            if (m_playheadFrame < frameMin) {
                m_playheadFrame = frameMin;
            }
            if (m_playheadFrame > frameMax) {
                m_playheadFrame = frameMax;
            }
        }

        const float timeSeconds = fps > 0.0f
            ? static_cast<float>(m_playheadFrame) / fps
            : 0.0f;

        if (PlaybackComponent* playback = m_registry->GetComponent<PlaybackComponent>(m_previewEntity)) {
            playback->currentSeconds = timeSeconds;
            if (clipLength > 0.0f) {
                playback->clipLength = clipLength;
            }
            playback->playing = m_isPlaying;
            if (m_previewState.IsActive()) {
                playback->looping = m_previewState.GetDriver()->IsLoop();
            }
            playback->stopAtEnd = !playback->looping;
            playback->finished = false;
        }

        if (TimelineComponent* timeline = m_registry->GetComponent<TimelineComponent>(m_previewEntity)) {
            timeline->frameMin = frameMin;
            timeline->frameMax = frameMax;
            timeline->currentFrame = m_playheadFrame;
            timeline->playing = m_isPlaying;
            if (clipLength > 0.0f) {
                timeline->clipLengthSec = clipLength;
            }
        }

        if (ColliderComponent* collider = m_registry->GetComponent<ColliderComponent>(m_previewEntity)) {
            collider->enabled = true;
            collider->drawGizmo = true;
        }
    }

    if (syncPreviewState) {
        PlayerEditorSession::SyncPreviewTimelinePlayback(*this);
    }
}

bool PlayerEditorPanel::SavePrefabDocument(bool saveAs)
{
    return PlayerEditorSession::SavePrefabDocument(*this, saveAs);
}

void PlayerEditorPanel::ImportFromSelectedEntity()
{
    PlayerEditorSession::ImportFromSelectedEntity(*this);
}

void PlayerEditorPanel::SetModel(const Model* model)
{
    if (m_model == model) return;
    Suspend();
    m_ownedModel.reset();
    m_model = model;
    ResetSelectionState();
}

// ============================================================================
// Toolbar / Empty-state placeholder
// ============================================================================

void PlayerEditorPanel::DrawToolbar()
{
    // v2.0 ActorEditor: mode dropdown.
    {
        const char* modeLabels[] = { "Player", "Enemy", "NPC" };
        int modeIdx = static_cast<int>(m_actorEditorMode);
        ImGui::PushItemWidth(110.0f);
        if (ImGui::Combo("Mode", &modeIdx, modeLabels, IM_ARRAYSIZE(modeLabels))) {
            const auto newMode = static_cast<ActorEditorMode>(modeIdx);
            if (newMode != m_actorEditorMode) {
                if (HasAnyDirtyDocument()) {
                    // Defer destructive switch: warn but still apply (Save/Discard
                    // dialog can be wired later. v2.0 minimum: log + proceed).
                }
                m_actorEditorMode = newMode;
                m_inlineBtExpanded = false;
                m_inlineBtLoaded   = false;
            }
        }
        ImGui::PopItemWidth();
        ImGui::SameLine();
    }

    if (DrawToolbarButton(ICON_FA_FOLDER_OPEN " Open")) {
        char pathBuffer[MAX_PATH] = {};
        if (!m_currentModelPath.empty()) {
            strcpy_s(pathBuffer, m_currentModelPath.c_str());
        }
        if (Dialog::OpenFileName(pathBuffer, MAX_PATH, kModelFileFilter, "Open Player Source") == DialogResult::OK) {
            OpenModelFromPath(pathBuffer);
        }
    }
    ImGui::SameLine();
    const bool canSaveWorkspace = CanUsePreviewEntity();
    if (DrawToolbarButton(ICON_FA_FLOPPY_DISK " Save", canSaveWorkspace)) {
        SavePrefabDocument(false);
    }

    // v2.0 ActorEditor: mode-specific Setup / Repair buttons.
    if (m_actorEditorMode == ActorEditorMode::Enemy && m_registry && CanUsePreviewEntity()) {
        ImGui::SameLine();
        if (DrawToolbarButton("Setup Full Enemy")) {
            extern void EnemyEditorSetupFullEnemy(class Registry&, EntityID, StateMachineAsset&);
            EnemyEditorSetupFullEnemy(*m_registry, m_previewEntity, m_stateMachineAsset);
            m_stateMachineDirty = true;
        }
        ImGui::SameLine();
        if (DrawToolbarButton("Repair Runtime")) {
            extern void EnemyEditorRepairRuntime(class Registry&, EntityID);
            EnemyEditorRepairRuntime(*m_registry, m_previewEntity);
        }
    } else if (m_actorEditorMode == ActorEditorMode::NPC && m_registry && CanUsePreviewEntity()) {
        ImGui::SameLine();
        if (DrawToolbarButton("Setup Full NPC")) {
            extern void EnemyEditorSetupFullNPC(class Registry&, EntityID, StateMachineAsset&);
            EnemyEditorSetupFullNPC(*m_registry, m_previewEntity, m_stateMachineAsset);
            m_stateMachineDirty = true;
        }
    }
}

void PlayerEditorPanel::DrawEmptyState()
{
    ImGui::Spacing();
    ImGui::TextDisabled("No model opened.");
    ImGui::TextWrapped("Open a model or prefab first. The current editor only becomes meaningful after a player source is resolved.");
    ImGui::Spacing();

    if (ImGui::Button(ICON_FA_FOLDER_OPEN " Open...", ImVec2(180.0f, 0.0f))) {
        char pathBuffer[MAX_PATH] = {};
        if (Dialog::OpenFileName(pathBuffer, MAX_PATH, kModelFileFilter, "Open Player Source") == DialogResult::OK) {
            OpenModelFromPath(pathBuffer);
        }
    }

    if (HasSelectedEntityContext()) {
        if (ImGui::Button(ICON_FA_ARROW_DOWN " Use Selected Entity Model", ImVec2(220.0f, 0.0f))) {
            ImportFromSelectedEntity();
        }
        ImGui::TextDisabled("Selected: %s", m_selectedEntityModelPath.c_str());
    } else {
        ImGui::TextDisabled("No selected entity with MeshComponent.modelFilePath.");
    }
}

// ============================================================================
// Top-level Draw entry points + DockSpace shell
// ============================================================================

void PlayerEditorPanel::Draw(Registry* registry, bool* p_open, bool* outFocused)
{
    DrawInternal(registry, p_open, outFocused, HostMode::Window);
}

void PlayerEditorPanel::DrawWorkspace(Registry* registry, bool* outFocused)
{
    DrawInternal(registry, nullptr, outFocused, HostMode::Workspace);
}

void PlayerEditorPanel::DrawDetached(Registry* registry, bool* p_open, bool* outFocused)
{
    DrawInternal(registry, p_open, outFocused, HostMode::Detached);
}

void PlayerEditorPanel::DrawInternal(Registry* registry, bool* p_open, bool* outFocused, HostMode hostMode)
{
    m_registry = registry;
    if (!Entity::IsNull(m_previewEntity) && (!m_registry || !m_registry->IsAlive(m_previewEntity))) {
        m_previewEntity = Entity::NULL_ID;
        m_previewEntityOwned = false;
    }
    EnsureOwnedPreviewEntity();

    if (hostMode != m_lastHostMode) {
        m_needsLayoutRebuild = true;
        m_lastHostMode = hostMode;
    }

    if (hostMode == HostMode::Workspace || hostMode == HostMode::Detached) {
        if (hostMode == HostMode::Detached) {
            const ImGuiViewport* viewport = ImGui::GetMainViewport();
            ImGui::SetNextWindowPos(viewport->Pos);
            ImGui::SetNextWindowSize(viewport->Size);
            ImGui::SetNextWindowViewport(viewport->ID);
        }
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));

        const ImGuiWindowFlags hostFlags =
            ImGuiWindowFlags_NoDocking |
            ImGuiWindowFlags_NoTitleBar |
            ImGuiWindowFlags_NoCollapse |
            ImGuiWindowFlags_NoResize |
            ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_NoBringToFrontOnFocus |
            ImGuiWindowFlags_NoNavFocus |
            ImGuiWindowFlags_NoScrollbar |
            ImGuiWindowFlags_NoScrollWithMouse;

        const char* hostName = (hostMode == HostMode::Detached)
            ? "##PlayerEditorDetachedRoot"
            : "##PlayerEditorWorkspaceRoot";
        const bool hostOpen = ImGui::Begin(hostName, nullptr, hostFlags);
        ImGui::PopStyleVar(3);
        if (!hostOpen) {
            ImGui::End();
            if (outFocused) *outFocused = false;
            return;
        }

        if (outFocused) *outFocused = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);

        if (hostMode == HostMode::Detached && !DrawDetachedTopTabBar(p_open)) {
            ImGui::End();
            if (outFocused) *outFocused = false;
            return;
        }

        DrawToolbar();
        ImGui::Separator();

        const char* dockName = (hostMode == HostMode::Detached)
            ? "PlayerEditorDetachedDock"
            : "PlayerEditorWorkspaceDock";
        ImGuiID dockId = ImGui::GetID(dockName);
        ImGui::DockSpace(dockId, ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_None);

        if (m_needsLayoutRebuild) {
            BuildDockLayout(dockId);
            m_needsLayoutRebuild = false;
        }

        ImGui::End();

        DrawSkeletonPanel();
        DrawViewportPanel();
        DrawStateMachinePanel();
        DrawTimelinePanel();
        DrawPropertiesPanel();
        DrawAnimatorPanel();
        DrawInputPanel();
        return;
    }

    ImGui::SetNextWindowSize(ImVec2(1200, 700), ImGuiCond_FirstUseEver);

    ImGuiWindowFlags hostFlags = ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse;
    if (!ImGui::Begin(ICON_FA_USER " Player Editor", p_open, hostFlags)) {
        ImGui::End();
        if (outFocused) *outFocused = false;
        return;
    }

    if (outFocused) *outFocused = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);
    DrawToolbar();
    ImGui::Separator();

    // ── Create internal DockSpace ──
    ImGuiID dockId = ImGui::GetID("PlayerEditorDock_v2");
    ImGui::DockSpace(dockId, ImVec2(0, 0), ImGuiDockNodeFlags_None);

    if (m_needsLayoutRebuild) {
        BuildDockLayout(dockId);
        m_needsLayoutRebuild = false;
    }

    ImGui::End(); // Host window

    // ── Draw each sub-window (they dock into the host's DockSpace) ──
    DrawSkeletonPanel();
    DrawViewportPanel();
    DrawStateMachinePanel();
    DrawTimelinePanel();
    DrawPropertiesPanel();
    DrawAnimatorPanel();
    DrawInputPanel();
}

// ============================================================================
// DockSpace Layout Builder  (UE Animation Editor style)
// ============================================================================

void PlayerEditorPanel::BuildDockLayout(unsigned int dockspaceId)
{
    ImGuiID dockId = dockspaceId;
    ImGui::DockBuilderRemoveNode(dockId);
    ImGui::DockBuilderAddNode(dockId, ImGuiDockNodeFlags_DockSpace);
    ImGui::DockBuilderSetNodeSize(dockId, ImVec2(1500, 850));

    ImGuiID topId = dockId;

    ImGuiID bottomId = 0;
    ImGui::DockBuilderSplitNode(topId, ImGuiDir_Down, 0.34f, &bottomId, &topId);

    ImGuiID skeletonId = 0;
    ImGui::DockBuilderSplitNode(topId, ImGuiDir_Left, 0.21f, &skeletonId, &topId);

    ImGuiID rightColumnId = 0;
    ImGui::DockBuilderSplitNode(topId, ImGuiDir_Right, 0.23f, &rightColumnId, &topId);

    ImGuiID viewportId = 0;
    ImGui::DockBuilderSplitNode(topId, ImGuiDir_Right, 0.34f, &viewportId, &topId);

    ImGuiID rightBottomId = 0;
    ImGui::DockBuilderSplitNode(rightColumnId, ImGuiDir_Down, 0.42f, &rightBottomId, &rightColumnId);

    ImGui::DockBuilderDockWindow(kPESkeletonTitle,     skeletonId);
    ImGui::DockBuilderDockWindow(kPEStateMachineTitle, topId);
    ImGui::DockBuilderDockWindow(kPEViewportTitle,     viewportId);
    ImGui::DockBuilderDockWindow(kPEPropertiesTitle,   rightColumnId);
    ImGui::DockBuilderDockWindow(kPEAnimatorTitle,     rightBottomId);
    ImGui::DockBuilderDockWindow(kPEInputTitle,        rightBottomId);
    ImGui::DockBuilderDockWindow(kPETimelineTitle,     bottomId);

    ImGui::DockBuilderFinish(dockId);
}
