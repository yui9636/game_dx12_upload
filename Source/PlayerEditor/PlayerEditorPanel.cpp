// PlayerEditor の中核パネル。ライフサイクル、上位 Draw / DockSpace、ツールバー、
// プレビューエンティティ / モデル設定、プレハブ保存の委譲を扱う。用途別の
// 描画処理は兄弟ファイルへ分割している:
//   PlayerEditorViewportPanel.cpp     — 3D ビューポート + カメラ補助
//   PlayerEditorSkeletonPanel.cpp     — ボーンツリー、ソケット、永続コライダー
//   PlayerEditorStateMachinePanel.cpp — ステートマシン UI + プリセット
//   PlayerEditorTimelinePanel.cpp     — タイムライン UI + 再生
//   PlayerEditorInspectorPanel.cpp    — プロパティ / アニメータ / 入力パネル
// 共有 static は PlayerEditorPanelInternal.{h,cpp} に置く。
#include "PlayerEditorPanel.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <DirectXMath.h>

#include <algorithm>
#include <cmath>

#include <imgui.h>
#include <imgui_internal.h>

#include "Icon/IconsFontAwesome7.h"
#include "PlayerEditorPanelInternal.h"
#include "PlayerEditorSession.h"
#include "Component/ColliderComponent.h"
#include "Gameplay/PlaybackComponent.h"
#include "Gameplay/PlayerRuntimeSetup.h"
#include "Gameplay/StateMachineParamsComponent.h"
#include "Gameplay/TimelineComponent.h"
#include "Model/Model.h"
#include "Registry/Registry.h"
#include "System/Dialog.h"

using namespace PlayerEditorInternal;
// ライフサイクル / プレビューエンティティ / 外部選択（セッション補助へ委譲）
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
    if (m_pendingCameraFitDistance > 0.0f) {
        m_vpCameraDist = m_pendingCameraFitDistance;
    }
    const float forwardLenSq =
        m_pendingCameraFitForward.x * m_pendingCameraFitForward.x +
        m_pendingCameraFitForward.y * m_pendingCameraFitForward.y +
        m_pendingCameraFitForward.z * m_pendingCameraFitForward.z;
    if (forwardLenSq > 0.0001f) {
        const float invLen = 1.0f / std::sqrt(forwardLenSq);
        const float fx = m_pendingCameraFitForward.x * invLen;
        const float fy = std::clamp(m_pendingCameraFitForward.y * invLen, -0.98f, 0.98f);
        const float fz = m_pendingCameraFitForward.z * invLen;
        m_vpCameraYaw = std::atan2(fx, fz);
        m_vpCameraPitch = std::asin(fy);
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
            timeline->previousFrame = timeline->currentFrame;
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
// ツールバー / 空状態プレースホルダー
void PlayerEditorPanel::DrawToolbar()
{
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted("Player");
    ImGui::SameLine();

    const char* modeLabels[] = { "Player", "Enemy", "NPC" };
    int modeIdx = static_cast<int>(m_actorEditorMode);
    ImGui::SetNextItemWidth(92.0f);
    if (ImGui::Combo("##ActorMode", &modeIdx, modeLabels, IM_ARRAYSIZE(modeLabels))) {
        const auto newMode = static_cast<ActorEditorMode>(modeIdx);
        if (newMode != m_actorEditorMode) {
            m_actorEditorMode = newMode;
            m_inlineBtExpanded = false;
            m_inlineBtLoaded = false;
        }
    }
    ImGui::SameLine();

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

    ImGui::SameLine();
    if (DrawToolbarButton("Setup Full Player", m_actorEditorMode == ActorEditorMode::Player && CanUsePreviewEntity())) {
        ApplyFullPlayerPreset();
    }
    ImGui::SameLine();
    if (DrawToolbarButton(ICON_FA_CAMERA " Fit", HasOpenModel())) {
        RequestCameraFit();
    }
    ImGui::SameLine();
    if (DrawToolbarButton(ICON_FA_ROTATE_LEFT " Reset", CanUsePreviewEntity())) {
        ResetPreviewRuntime();
    }

    ImGui::SameLine();
    ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
    ImGui::SameLine();

    const bool editMode = m_viewMode == PlayerEditorViewMode::Edit;
    if (!editMode) {
        ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));
    }
    if (ImGui::Button("Test")) {
        m_viewMode = PlayerEditorViewMode::Test;
        m_toolPopoverOpen = false;
    }
    if (!editMode) {
        ImGui::PopStyleColor();
    }
    ImGui::SameLine();
    if (editMode) {
        ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));
    }
    if (ImGui::Button("Edit")) {
        m_viewMode = PlayerEditorViewMode::Edit;
        m_toolPopoverOpen = false;
    }
    if (editMode) {
        ImGui::PopStyleColor();
    }

    ImGui::SameLine();
    ImGui::TextDisabled("Tool: %s", m_toolPopoverOpen ? GetActiveToolLabel() : "Viewport");

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

void PlayerEditorPanel::DrawMainWorkspace()
{
    DrawToolbar();
    ImGui::Separator();
    DrawViewportSurface();
    DrawToolPopover();
}

const char* PlayerEditorPanel::GetActiveToolLabel() const
{
    switch (m_activeTool) {
    case PlayerEditorTool::State: return "State";
    case PlayerEditorTool::Timeline: return "Timeline";
    case PlayerEditorTool::Hitbox: return "Hitbox";
    case PlayerEditorTool::Body: return "Body";
    case PlayerEditorTool::Input: return "Input";
    case PlayerEditorTool::Bone: return "Bone";
    default: return "Tool";
    }
}

void PlayerEditorPanel::RequestCameraFit()
{
    if (!m_model) {
        return;
    }

    const auto bounds = m_model->GetWorldBounds();
    const DirectX::XMFLOAT3 ex = bounds.Extents;
    const float radius = (std::max)(std::sqrt(ex.x * ex.x + ex.y * ex.y + ex.z * ex.z), 1.0f);
    const float pitch = DirectX::XMConvertToRadians(-12.0f);
    const float yaw = 0.0f;
    const float halfFov = 0.785398f * 0.5f;
    const float distance = (std::max)((radius / (std::max)(std::sin(halfFov), 0.001f)) * 1.25f, 2.5f);

    m_vpCameraYaw = yaw;
    m_vpCameraPitch = pitch;
    m_vpCameraDist = distance;

    m_pendingCameraFitTarget = bounds.Center;
    m_pendingCameraFitRadius = radius;
    m_pendingCameraFitForward = {
        std::sin(yaw) * std::cos(pitch),
        std::sin(pitch),
        std::cos(yaw) * std::cos(pitch)
    };
    m_pendingCameraFitDistance = distance;
    m_hasPendingCameraFit = true;
}

void PlayerEditorPanel::ResetPreviewRuntime()
{
    m_playheadFrame = 0;
    m_isPlaying = false;
    if (m_previewState.IsActive()) {
        m_previewState.ExitPreview();
    }
    if (CanUsePreviewEntity()) {
        PlayerRuntimeSetup::ResetPlayerRuntimeState(*m_registry, m_previewEntity);
    }
    SyncPreviewTimelinePlayback();
    RequestCameraFit();
}

void PlayerEditorPanel::DrawStateTool()
{
    DrawStateMachineRuntimeStatus();
    ImGui::Separator();
    DrawStateMachineParameterList();
    ImGui::Separator();
    DrawStateNodeInspector();
}

void PlayerEditorPanel::DrawTimelineTool()
{
    DrawTimelinePlaybackToolbar();
    ImGui::Separator();
    const float headerHeight = 86.0f;
    DrawTimelineTrackHeaders(headerHeight);
    ImGui::SameLine();
    DrawTimelineGrid(headerHeight);
    ImGui::Separator();
    DrawTimelineItemInspector();
}

void PlayerEditorPanel::DrawHitboxTool()
{
    if (ImGui::Button(ICON_FA_PLUS " Attack", ImVec2(-1.0f, 0.0f)) && HasOpenModel() && CanUsePreviewEntity()) {
        AddPersistentCollider(ColliderAttribute::Attack);
    }
    ImGui::Checkbox("Show hitboxes", &m_overlayHitboxes);
    ImGui::Separator();
    DrawPersistentColliderSection();
    DrawPersistentColliderInspector();
}

void PlayerEditorPanel::DrawBodyTool()
{
    if (ImGui::Button(ICON_FA_PLUS " Body", ImVec2(-1.0f, 0.0f)) && HasOpenModel() && CanUsePreviewEntity()) {
        AddPersistentCollider(ColliderAttribute::Body);
    }
    ImGui::Checkbox("Show runtime", &m_overlayRuntime);
    ImGui::Separator();
    DrawPersistentColliderSection();
    DrawPersistentColliderInspector();
}

void PlayerEditorPanel::DrawInputTool()
{
    if (m_inputMappingTab.Draw(m_registry)) {
        ApplyEditorBindingsToPreviewEntity();
    }
}

void PlayerEditorPanel::DrawBoneTool()
{
    ImGui::SetNextItemWidth(-1.0f);
    ImGui::InputTextWithHint("##BoneSearch", "Search bones...", m_boneSearchFilter, sizeof(m_boneSearchFilter));
    ImGui::Separator();
    if (m_model) {
        ImGui::BeginChild("##BoneTreeCompact", ImVec2(0.0f, 220.0f), true);
        const auto& nodes = m_model->GetNodes();
        if (m_boneSearchFilter[0] == '\0') {
            for (int i = 0; i < static_cast<int>(nodes.size()); ++i) {
                if (nodes[i].parentIndex < 0) {
                    DrawBoneTreeNode(i);
                }
            }
        }
        else {
            const std::string filter(m_boneSearchFilter);
            for (int i = 0; i < static_cast<int>(nodes.size()); ++i) {
                if (nodes[i].name.find(filter) == std::string::npos) {
                    continue;
                }
                const bool selected = (m_selectedBoneIndex == i);
                if (ImGui::Selectable(("[" + std::to_string(i) + "] " + nodes[i].name).c_str(), selected)) {
                    m_selectedBoneIndex = i;
                    m_selectedBoneName = nodes[i].name;
                    if (!TryAssignSelectedBoneToTimelineItem(i) && !TryAssignSelectedBoneToPersistentCollider(i)) {
                        m_selectionCtx = SelectionContext::Bone;
                    }
                }
            }
        }
        ImGui::EndChild();
    }
    else {
        ImGui::TextDisabled("No model.");
    }
    ImGui::Separator();
    DrawSocketList(120.0f);
}

void PlayerEditorPanel::DrawToolPopover()
{
    if (!m_toolPopoverOpen || m_viewMode == PlayerEditorViewMode::Test || m_activeTool == PlayerEditorTool::None) {
        return;
    }

    const float viewportX = m_viewportRect.x;
    const float viewportY = m_viewportRect.y;
    const float viewportW = m_viewportRect.z;
    const float viewportH = m_viewportRect.w;
    const ImVec2 pos(viewportX + (std::max)(12.0f, viewportW - 430.0f), viewportY + 12.0f);
    const float popoverW = (std::min)(420.0f, (std::max)(280.0f, viewportW - 24.0f));
    const float popoverH = (std::max)(180.0f, (std::min)(viewportH - 24.0f, 560.0f));
    const ImVec2 size(popoverW, popoverH);

    ImGui::SetNextWindowPos(pos, ImGuiCond_Always);
    ImGui::SetNextWindowSize(size, ImGuiCond_Always);
    ImGuiWindowFlags flags =
        ImGuiWindowFlags_NoDocking |
        ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_NoSavedSettings;

    bool open = true;
    std::string title = std::string(GetActiveToolLabel()) + "##PlayerEditorToolPopover";
    if (!ImGui::Begin(title.c_str(), &open, flags)) {
        ImGui::End();
        m_toolPopoverOpen = open;
        return;
    }

    switch (m_activeTool) {
    case PlayerEditorTool::State:
        DrawStateTool();
        break;
    case PlayerEditorTool::Timeline:
        DrawTimelineTool();
        break;
    case PlayerEditorTool::Hitbox:
        DrawHitboxTool();
        break;
    case PlayerEditorTool::Body:
        DrawBodyTool();
        break;
    case PlayerEditorTool::Input:
        DrawInputTool();
        break;
    case PlayerEditorTool::Bone:
        DrawBoneTool();
        break;
    default:
        break;
    }

    ImGui::End();
    m_toolPopoverOpen = open;
}

// 上位 Draw 入口と DockSpace の外枠
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

        DrawMainWorkspace();

        ImGui::End();
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
    DrawMainWorkspace();

    ImGui::End(); // ホストウィンドウ
}
// DockSpace レイアウト構築（UE Animation Editor 風）
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
