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
#include "EditorTheme.h"
#include "Component/ColliderComponent.h"
#include "Gameplay/PlaybackComponent.h"
#include "Gameplay/PlayerRuntimeSetup.h"
#include "Gameplay/StateMachineParamsComponent.h"
#include "Gameplay/TimelineComponent.h"
#include "Model/Model.h"
#include "Registry/Registry.h"
#include "System/Dialog.h"
#include "System/UndoSystem.h"

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
    // 自動 Fit でパンオフセットを必ずリセット
    m_vpCameraPanOffset = { 0.0f, 0.0f, 0.0f };
    // 軌道中心も pending から確定
    m_orbitCenter = m_pendingCameraFitTarget;
    m_orbitCenterValid = true;
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
    m_orbitCenterValid = false;  // モデル変更で軌道中心リセット
    if (m_model) {
        // 読込直後はバウンドが未確定なことがあるので数フレーム再 Fit
        m_autoFitCountdown = 8;
    }
}
// コンパクトな縦スタックボタン (アイコン 14px + 8pt ラベル、30px 高)
bool PlayerEditorPanel::DrawToolbarInlineButton(const char* icon, const char* label, const char* id,
                                                  bool active, bool enabled, bool dirtyDot)
{
    constexpr float kIconSize = 14.0f;
    constexpr float kBtnH     = 30.0f;
    constexpr float kPadX     = 5.0f;
    constexpr float kMinW     = 30.0f;

    if (!enabled) ImGui::BeginDisabled();
    ImGui::PushID(id);

    const ImVec2 lblSize  = (label && label[0]) ? ImGui::CalcTextSize(label) : ImVec2(0, 0);
    const float btnW = (std::max)(kMinW, kPadX * 2.0f + lblSize.x);
    const ImVec2 cursor = ImGui::GetCursorScreenPos();

    ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0, 0, 0, 0));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.173f, 0.173f, 0.173f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive,  ImVec4(0.000f, 0.439f, 0.878f, 1.0f));
    const bool pressed = ImGui::Button("##b", ImVec2(btnW, kBtnH));
    const bool hovered = ImGui::IsItemHovered();
    ImGui::PopStyleColor(3);

    ImDrawList* dl = ImGui::GetWindowDrawList();
    if (active) {
        dl->AddRectFilled(cursor, ImVec2(cursor.x + btnW, cursor.y + kBtnH),
                          IM_COL32(0, 112, 224, 255));
    } else if (hovered) {
        dl->AddRectFilled(cursor, ImVec2(cursor.x + btnW, cursor.y + kBtnH),
                          IM_COL32(44, 44, 44, 255));
    }
    const ImU32 textColor = active ? IM_COL32(255, 255, 255, 255) : IM_COL32(200, 200, 200, 255);
    if (icon && icon[0]) {
        ImFont* font = ImGui::GetFont();
        const ImVec2 iconDim = font->CalcTextSizeA(kIconSize, FLT_MAX, 0.0f, icon);
        dl->AddText(font, kIconSize,
            ImVec2(cursor.x + (btnW - iconDim.x) * 0.5f, cursor.y + 2.0f),
            textColor, icon);
    }
    if (label && label[0]) {
        dl->AddText(ImVec2(cursor.x + (btnW - lblSize.x) * 0.5f, cursor.y + kBtnH - lblSize.y - 2.0f),
                    textColor, label);
    }
    if (dirtyDot) {
        dl->AddCircleFilled(ImVec2(cursor.x + btnW - 4.0f, cursor.y + 4.0f),
                            2.5f, IM_COL32(255, 165, 50, 255), 8);
    }

    ImGui::PopID();
    if (!enabled) ImGui::EndDisabled();
    return enabled && pressed;
}

void PlayerEditorPanel::DrawTopBar()
{
    constexpr float kBarH = 34.0f;
    constexpr float kGap  = 2.0f;

    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.122f, 0.122f, 0.122f, 1.0f));
    ImGui::BeginChild("##PETopBar", ImVec2(0.0f, kBarH), false,
        ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    ImGui::PopStyleColor();

    ImDrawList* dl = ImGui::GetWindowDrawList();
    const ImVec2 barOrigin = ImGui::GetCursorScreenPos();
    const float barWidth = ImGui::GetContentRegionAvail().x;

    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(kGap, 0.0f));

    const auto sep = [&]() {
        ImGui::SameLine(0.0f, 4.0f);
        const ImVec2 sp = ImGui::GetCursorScreenPos();
        dl->AddLine(ImVec2(sp.x, sp.y + 4.0f),
                    ImVec2(sp.x, sp.y + kBarH - 8.0f),
                    IM_COL32(56, 56, 56, 255), 1.0f);
        ImGui::Dummy(ImVec2(1.0f, kBarH - 6.0f));
        ImGui::SameLine(0.0f, 4.0f);
    };

    // G1: Menu (☰)
    if (DrawToolbarInlineButton(ICON_FA_BARS, nullptr, "##tbMenu", false, true, false)) {
        ImGui::OpenPopup("##PEMenuPopup");
    }
    if (ImGui::BeginPopup("##PEMenuPopup")) {
        if (ImGui::MenuItem(ICON_FA_FOLDER_OPEN " Open...", "Ctrl+O")) {
            char pathBuffer[MAX_PATH] = {};
            if (!m_currentModelPath.empty()) strcpy_s(pathBuffer, m_currentModelPath.c_str());
            if (Dialog::OpenFileName(pathBuffer, MAX_PATH, kModelFileFilter, "Open Player Source") == DialogResult::OK) {
                OpenModelFromPath(pathBuffer);
            }
        }
        ImGui::Separator();
        if (ImGui::MenuItem(ICON_FA_FLOPPY_DISK " Save", "Ctrl+S", false, CanUsePreviewEntity())) SavePrefabDocument(false);
        if (ImGui::MenuItem(ICON_FA_FILE_EXPORT " Save As...", "Ctrl+Shift+S", false, CanUsePreviewEntity())) SavePrefabDocument(true);
        ImGui::EndPopup();
    }
    sep();

    // G2: Open のみ (Save は ☰ メニューへ集約)
    {
        if (DrawToolbarInlineButton(ICON_FA_FOLDER_OPEN, "Open", "##tbOpen", false, true, false)) {
            char pathBuffer[MAX_PATH] = {};
            if (!m_currentModelPath.empty()) strcpy_s(pathBuffer, m_currentModelPath.c_str());
            if (Dialog::OpenFileName(pathBuffer, MAX_PATH, kModelFileFilter, "Open Player Source") == DialogResult::OK) {
                OpenModelFromPath(pathBuffer);
            }
        }
    }
    sep();

    // G3: Mode (Player / Enemy / NPC) — 単一ボタンで dropdown
    {
        const char* modeLabels[] = { "Player", "Enemy", "NPC" };
        const char* curLabel = modeLabels[(int)m_actorEditorMode];
        if (DrawToolbarInlineButton(ICON_FA_USER, curLabel, "##tbMode", false, true, false)) {
            ImGui::OpenPopup("##PEModePopup");
        }
        if (ImGui::BeginPopup("##PEModePopup")) {
            for (int i = 0; i < 3; ++i) {
                if (ImGui::MenuItem(modeLabels[i], nullptr, (int)m_actorEditorMode == i)) {
                    if (m_actorEditorMode != (ActorEditorMode)i) {
                        m_actorEditorMode = (ActorEditorMode)i;
                        m_inlineBtExpanded = false;
                        m_inlineBtLoaded = false;
                    }
                }
            }
            ImGui::EndPopup();
        }
    }
    sep();

    // G4: Play / Edit
    {
        const bool isTest = m_viewMode == PlayerEditorViewMode::Test;
        if (DrawToolbarInlineButton(ICON_FA_PLAY, "Test", "##tbTest", isTest, true, false)) m_viewMode = PlayerEditorViewMode::Test;
        ImGui::SameLine(0.0f, kGap);
        if (DrawToolbarInlineButton(ICON_FA_PEN, "Edit", "##tbEdit", !isTest, true, false)) m_viewMode = PlayerEditorViewMode::Edit;
    }
    sep();

    // G5: Full Player (Fit は自動化、ボタン廃止)
    {
        const bool canFull = (m_actorEditorMode == ActorEditorMode::Player) && CanUsePreviewEntity();
        if (DrawToolbarInlineButton(ICON_FA_BOLT, "Full", "##tbFull", false, canFull, false)) ApplyFullPlayerPreset();
    }

    ImGui::PopStyleVar();
    ImGui::EndChild();

    dl->AddLine(ImVec2(barOrigin.x, barOrigin.y + kBarH - 0.5f),
                ImVec2(barOrigin.x + barWidth, barOrigin.y + kBarH - 0.5f),
                IM_COL32(13, 13, 13, 255), 1.0f);
}

void PlayerEditorPanel::DrawLeftSidebar(float w)
{
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    ImGui::BeginChild("##PELeftSidebar", ImVec2(w, 0.0f), true);
    ImGui::PopStyleVar();

    if (ImGui::BeginTabBar("##LSTabs", ImGuiTabBarFlags_NoCloseWithMiddleMouseButton)) {
        if (ImGui::BeginTabItem(ICON_FA_BONE " Skeleton Tree")) {
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(4.0f, 4.0f));
            ImGui::BeginChild("##LSBoneInner", ImVec2(0.0f, 0.0f), false);
            DrawSkeletonPanel();
            ImGui::EndChild();
            ImGui::PopStyleVar();
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem(ICON_FA_FOLDER_TREE " Asset Browser")) {
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(4.0f, 4.0f));
            ImGui::BeginChild("##LSAnimInner", ImVec2(0.0f, 0.0f), false);
            DrawAnimatorPanel();
            ImGui::EndChild();
            ImGui::PopStyleVar();
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }
    ImGui::EndChild();
}

void PlayerEditorPanel::DrawWorkbench()
{
    static const char* const tabLabels[4] = { "State##wb", "Attack##wb", "Hit##wb", "Input##wb" };

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    ImGui::BeginChild("##PEWorkbench", ImVec2(0.0f, 0.0f), true);
    ImGui::PopStyleVar();

    if (ImGui::BeginTabBar("##PEWorkbenchTabs", ImGuiTabBarFlags_NoCloseWithMiddleMouseButton)) {
        for (int i = 0; i < 4; ++i) {
            ImGuiTabItemFlags flags = ImGuiTabItemFlags_None;
            if (i == m_workbenchActiveTab) flags |= ImGuiTabItemFlags_SetSelected;
            if (ImGui::BeginTabItem(tabLabels[i], nullptr, flags)) {
                if (ImGui::IsItemActive() || ImGui::IsItemActivated()) {
                    m_workbenchActiveTab = i;
                }
                const ImVec2 tabMin = ImGui::GetItemRectMin();
                const ImVec2 tabMax = ImGui::GetItemRectMax();
                ImGui::GetWindowDrawList()->AddRectFilled(
                    ImVec2(tabMin.x, tabMax.y - 2.0f),
                    ImVec2(tabMax.x, tabMax.y),
                    IM_COL32(0, 112, 224, 255));

                ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(6.0f, 6.0f));
                ImGui::BeginChild("##wbTabBody", ImVec2(0.0f, 0.0f), false);
                switch (i) {
                case 0: DrawStateMachinePanel(); break;
                case 1: DrawTimelinePanel(); break;
                case 2:
                {
                    // Hit タブ: +Collider 1 ボタン + 一覧 + Inspector (Attack/Body は Inspector で切替)
                    const bool canAdd = HasOpenModel() && CanUsePreviewEntity();
                    if (!canAdd) {
                        ImGui::TextDisabled("Open a model and preview entity to edit colliders.");
                        break;
                    }
                    if (ImGui::Button(ICON_FA_PLUS " Collider")) {
                        AddPersistentCollider(ColliderAttribute::Body);  // 既定は Body、Inspector で切替
                    }
                    ImGui::Separator();

                    const float availY = ImGui::GetContentRegionAvail().y;
                    const float listH = (std::max)(120.0f, availY * 0.45f);
                    ImGui::BeginChild("##HitList", ImVec2(0.0f, listH), true);
                    DrawPersistentColliderSection();
                    ImGui::EndChild();
                    ImGui::BeginChild("##HitInspector", ImVec2(0.0f, 0.0f), true);
                    DrawPersistentColliderInspector();
                    ImGui::EndChild();
                    break;
                }
                case 3: DrawInputPanel(); break;
                default: break;
                }
                ImGui::EndChild();
                ImGui::PopStyleVar();
                ImGui::EndTabItem();
            }
        }
        ImGui::EndTabBar();
    }
    ImGui::EndChild();
}

void PlayerEditorPanel::DrawRightInspector(float w)
{
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    ImGui::BeginChild("##PERightInspector", ImVec2(w, 0.0f), true);
    ImGui::PopStyleVar();

    if (ImGui::BeginTabBar("##RITabs", ImGuiTabBarFlags_NoCloseWithMiddleMouseButton)) {
        if (ImGui::BeginTabItem(ICON_FA_SLIDERS " Details")) {
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(6.0f, 6.0f));
            ImGui::BeginChild("##RIDetails", ImVec2(0.0f, 0.0f), false);
            if (m_selectionCtx == SelectionContext::None) {
                ImGui::TextDisabled("Select an item to view details.");
            } else {
                DrawPropertiesPanel();
            }
            ImGui::EndChild();
            ImGui::PopStyleVar();
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }
    ImGui::EndChild();
}

void PlayerEditorPanel::DrawStatusBar()
{
    constexpr float kStatusH = 22.0f;
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.122f, 0.122f, 0.122f, 1.0f));
    ImGui::BeginChild("##PEStatus", ImVec2(0.0f, kStatusH), false,
        ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    ImGui::PopStyleColor();

    ImDrawList* dl = ImGui::GetWindowDrawList();
    const ImVec2 p = ImGui::GetCursorScreenPos();
    const float w = ImGui::GetContentRegionAvail().x;
    dl->AddLine(p, ImVec2(p.x + w, p.y), IM_COL32(13, 13, 13, 255), 1.0f);

    ImGui::AlignTextToFramePadding();
    if (HasAnyDirtyDocument()) {
        ImGui::TextColored(ImVec4(0.040f, 0.450f, 0.900f, 1.0f), ICON_FA_CIRCLE_DOT " Modified");
    } else {
        ImGui::TextDisabled(ICON_FA_CHECK " Clean");
    }
    ImGui::SameLine(0.0f, 16.0f);
    if (!m_currentModelPath.empty()) ImGui::TextDisabled("%s", m_currentModelPath.c_str());
    ImGui::EndChild();
}

void PlayerEditorPanel::HandleGlobalShortcuts()
{
    if (!ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows)) return;
    ImGuiIO& io = ImGui::GetIO();
    if (!io.KeyCtrl) return;
    if (ImGui::IsKeyPressed(ImGuiKey_S, false)) {
        if (CanUsePreviewEntity()) SavePrefabDocument(io.KeyShift);
    }
    if (ImGui::IsKeyPressed(ImGuiKey_Z, false) && m_registry) {
        if (io.KeyShift) UndoSystem::Instance().Redo(*m_registry);
        else UndoSystem::Instance().Undo(*m_registry);
    }
    if (ImGui::IsKeyPressed(ImGuiKey_Y, false) && m_registry) {
        UndoSystem::Instance().Redo(*m_registry);
    }
}

void PlayerEditorPanel::DrawMainWorkspace()
{
    PushPlayerEditorPanelStyle();
    PushPlayerEditorPanelSizeStyle();

    DrawTopBar();
    HandleGlobalShortcuts();

    // 読込直後のバウンド未確定を吸収する自動 Fit
    if (m_autoFitCountdown > 0 && m_model) {
        RequestCameraFit();
        --m_autoFitCountdown;
    }

    constexpr float kLeftW   = 240.0f;
    constexpr float kRightW  = 300.0f;
    constexpr float kStatusH = 22.0f;
    constexpr float kGap     = 1.0f;
    const float availH = ImGui::GetContentRegionAvail().y - kStatusH - kGap;
    const float workbenchH = m_workbenchOpen ? (std::max)(180.0f, availH * 0.30f) : 0.0f;
    const float viewportH  = (std::max)(120.0f, availH - workbenchH - (m_workbenchOpen ? kGap : 0.0f));

    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(kGap, kGap));

    ImGui::BeginGroup();
    DrawLeftSidebar(kLeftW);
    ImGui::SameLine(0.0f, kGap);

    const float centerW = ImGui::GetContentRegionAvail().x - kRightW - kGap;
    ImGui::BeginChild("##PECenter", ImVec2(centerW, 0.0f), false,
        ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.059f, 0.059f, 0.059f, 1.0f));
    ImGui::BeginChild("##PEViewportHost", ImVec2(0.0f, viewportH), true);
    DrawViewportSurface();
    ImGui::EndChild();
    ImGui::PopStyleColor();
    ImGui::PopStyleVar();

    if (m_workbenchOpen) {
        DrawWorkbench();
    } else {
        if (ImGui::Button(ICON_FA_CHEVRON_UP " State / Attack / Hit / Input", ImVec2(0.0f, 22.0f))) m_workbenchOpen = true;
    }
    ImGui::EndChild();

    ImGui::SameLine(0.0f, kGap);
    DrawRightInspector(kRightW);
    ImGui::EndGroup();

    DrawStatusBar();

    ImGui::PopStyleVar();
    PopPlayerEditorPanelSizeStyle();
    PopPlayerEditorPanelStyle();
}

void PlayerEditorPanel::RequestCameraFit()
{
    if (!m_model) {
        return;
    }

    const auto bounds = m_model->GetWorldBounds();
    const DirectX::XMFLOAT3 ex = bounds.Extents;
    // バウンディング球半径。1.0 にクランプせず実寸を尊重 (小型モデル対応)
    const float radius = (std::max)(std::sqrt(ex.x * ex.x + ex.y * ex.y + ex.z * ex.z), 0.05f);
    // 水平視 + 1.2 倍マージン、min distance 0.8 (近接クリップ回避)
    const float pitch = 0.0f;
    const float yaw = 0.0f;
    const float halfFov = 0.785398f * 0.5f;
    const float distance = (std::max)((radius / (std::max)(std::sin(halfFov), 0.001f)) * 1.2f, 0.8f);

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

    // 軌道中心をスナップショット (アニメ中は固定)
    m_orbitCenter = bounds.Center;
    m_orbitCenterValid = true;
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
