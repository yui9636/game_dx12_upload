#include "PlayerEditorPanel.h"
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <imgui.h>
#include <imgui_internal.h>
#include <algorithm>
#include <cctype>
#include <cfloat>
#include <cmath>
#include <filesystem>
#include <initializer_list>
#include "Icon/IconsFontAwesome7.h"
#include "TimelineAssetSerializer.h"
#include "StateMachineAssetSerializer.h"
#include "PlayerEditorSession.h"
#include "ImGuiRenderer.h"
#include "Model/Model.h"
#include "Component/MeshComponent.h"
#include "Component/NodeSocketComponent.h"
#include "Component/EffectPreviewTagComponent.h"
#include "Component/PrefabInstanceComponent.h"
#include "Component/TransformComponent.h"
#include "Component/NameComponent.h"
#include "Asset/PrefabSystem.h"
#include "Animator/AnimatorService.h"
#include "Gameplay/PlayerRuntimeSetup.h"
#include "Gameplay/StateMachineSystem.h"
#include "Gameplay/StateMachineParamsComponent.h"
#include "Gameplay/PlaybackComponent.h"
#include "Gameplay/TimelineAssetRuntimeBuilder.h"
#include "Gameplay/TimelineComponent.h"
#include "Gameplay/TimelineItemBuffer.h"
#include "Input/InputBindingComponent.h"
#include "Registry/Registry.h"
#include "System/Dialog.h"
#include "System/ResourceManager.h"

// ============================================================================
// Constants
// ============================================================================

static constexpr float kTrackHeaderWidth   = 160.0f;
static constexpr float kTrackHeight        = 26.0f;
static constexpr float kMinPixelsPerFrame  = 2.0f;
static constexpr float kDefaultPPF         = 5.0f;
static constexpr float kPlayheadTriSize    = 8.0f;
static constexpr float kDetachedTopTabHeight = 34.0f;

static constexpr float kNodeWidth  = 150.0f;
static constexpr float kNodeHeight = 38.0f;
static constexpr const char* kModelFileFilter =
    "Model Files (*.fbx;*.gltf;*.glb;*.obj)\0*.fbx;*.gltf;*.glb;*.obj\0All Files (*.*)\0*.*\0";
static constexpr const char* kTimelineFileFilter =
    "Timeline (*.timeline.json)\0*.timeline.json\0JSON (*.json)\0*.json\0All Files (*.*)\0*.*\0";
static constexpr const char* kStateMachineFileFilter =
    "StateMachine (*.statemachine.json)\0*.statemachine.json\0JSON (*.json)\0*.json\0All Files (*.*)\0*.*\0";
static constexpr const char* kInputMapFileFilter =
    "Input Map (*.inputmap.json)\0*.inputmap.json\0JSON (*.json)\0*.json\0All Files (*.*)\0*.*\0";
static constexpr const char* kPrefabFileFilter =
    "Prefab (*.prefab)\0*.prefab\0All Files (*.*)\0*.*\0";

// ── Window titles (used by DockBuilder) ──
static constexpr const char* kPEViewportTitle     = ICON_FA_CUBE " Viewport##PE";
static constexpr const char* kPESkeletonTitle     = ICON_FA_BONE " Skeleton##PE";
static constexpr const char* kPEStateMachineTitle = ICON_FA_DIAGRAM_PROJECT " State Machine##PE";
static constexpr const char* kPETimelineTitle     = ICON_FA_TIMELINE " Timeline##PE";
static constexpr const char* kPEPropertiesTitle   = ICON_FA_SLIDERS " Properties##PE";
static constexpr const char* kPEAnimatorTitle     = ICON_FA_PERSON_RUNNING " Animator##PE";
static constexpr const char* kPEInputTitle        = ICON_FA_GAMEPAD " Input##PE";

static const char* kTrackTypeNames[] = {
    "Animation", "Hitbox", "VFX", "Audio", "CameraShake", "Camera", "Event", "Custom"
};

static const char* GetStateTypeLabel(StateNodeType type)
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

static std::string GenerateDefaultTrackName(const TimelineAsset& asset, TimelineTrackType type)
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
    snprintf(buffer, sizeof(buffer), "%s %02d", baseName, nextIndex);
    return buffer;
}

static TimelineItem CreateDefaultTimelineItem(TimelineTrackType type, int startFrame)
{
    TimelineItem item;
    item.startFrame = (std::max)(0, startFrame);

    switch (type) {
    case TimelineTrackType::Hitbox:
        item.endFrame = item.startFrame + 8;
        item.hitbox.radius = 0.5f;
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

static const char* ResolveConditionTypeLabel(ConditionType type)
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

static const char* ResolveCompareOpLabel(CompareOp compare)
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

static bool HasExtension(const std::string& path, std::initializer_list<const char*> extensions)
{
    std::string ext = std::filesystem::path(path).extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    for (const char* candidate : extensions) {
        if (ext == candidate) {
            return true;
        }
    }
    return false;
}

static ImU32 StateNodeColor(StateNodeType type)
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

static bool ProjectBoneMarkerToViewport(
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
    const XMVECTOR at = XMLoadFloat3(&cameraTarget);
    const XMVECTOR up = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
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

static bool DrawDetachedTopTabBar(bool* p_open)
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

bool PlayerEditorPanel::DrawDocumentPathLabel(const char* label, const std::string& path, bool dirty)
{
    if (path.empty() && !dirty) {
        return false;
    }
    ImGui::TextDisabled("%s", label);
    ImGui::SameLine();
    if (dirty) {
        ImGui::TextColored(ImVec4(1.0f, 0.75f, 0.15f, 1.0f), "*");
        ImGui::SameLine(0.0f, 2.0f);
    }
    ImGui::TextUnformatted(path.empty() ? "(unsaved)" : path.c_str());
    return !path.empty();
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
    m_playheadFrame = 0;
    m_isPlaying = false;
}

bool PlayerEditorPanel::HasOpenModel() const
{
    return m_model != nullptr;
}

bool PlayerEditorPanel::HasAnyDirtyDocument() const
{
    return m_timelineDirty || m_stateMachineDirty || m_socketDirty || m_inputMappingTab.IsDirty();
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

bool PlayerEditorPanel::OpenTimelineFromPath(const std::string& path)
{
    return PlayerEditorSession::OpenTimelineFromPath(*this, path);
}

bool PlayerEditorPanel::OpenStateMachineFromPath(const std::string& path)
{
    return PlayerEditorSession::OpenStateMachineFromPath(*this, path);
}

bool PlayerEditorPanel::OpenInputMapFromPath(const std::string& path)
{
    return PlayerEditorSession::OpenInputMapFromPath(*this, path);
}

bool PlayerEditorPanel::SaveTimelineDocument(bool saveAs)
{
    return PlayerEditorSession::SaveTimelineDocument(*this, saveAs);
}

bool PlayerEditorPanel::SaveStateMachineDocument(bool saveAs)
{
    return PlayerEditorSession::SaveStateMachineDocument(*this, saveAs);
}

bool PlayerEditorPanel::SaveInputMapDocument(bool saveAs)
{
    return PlayerEditorSession::SaveInputMapDocument(*this, saveAs);
}

bool PlayerEditorPanel::SaveAllDocuments(bool saveAs)
{
    return PlayerEditorSession::SaveAllDocuments(*this, saveAs);
}

void PlayerEditorPanel::ApplyEditorBindingsToPreviewEntity()
{
    PlayerEditorSession::ApplyEditorBindingsToPreviewEntity(*this);
}

void PlayerEditorPanel::RebuildPreviewTimelineRuntimeData()
{
    PlayerEditorSession::RebuildPreviewTimelineRuntimeData(*this);
}

void PlayerEditorPanel::SyncPreviewTimelinePlayback()
{
    PlayerEditorSession::SyncPreviewTimelinePlayback(*this);
}

bool PlayerEditorPanel::SavePrefabDocument(bool saveAs)
{
    return PlayerEditorSession::SavePrefabDocument(*this, saveAs);
}

void PlayerEditorPanel::RevertAllDocuments()
{
    PlayerEditorSession::RevertAllDocuments(*this);
}

void PlayerEditorPanel::ImportSocketsFromPreviewEntity()
{
    PlayerEditorSession::ImportSocketsFromPreviewEntity(*this);
}

void PlayerEditorPanel::ExportSocketsToPreviewEntity()
{
    PlayerEditorSession::ExportSocketsToPreviewEntity(*this);
}

void PlayerEditorPanel::ImportFromSelectedEntity()
{
    PlayerEditorSession::ImportFromSelectedEntity(*this);
}

void PlayerEditorPanel::DrawToolbar()
{
    if (DrawToolbarButton(ICON_FA_FOLDER_OPEN " Open Model")) {
        char pathBuffer[MAX_PATH] = {};
        if (!m_currentModelPath.empty()) {
            strcpy_s(pathBuffer, m_currentModelPath.c_str());
        }
        if (Dialog::OpenFileName(pathBuffer, MAX_PATH, kModelFileFilter, "Open Model") == DialogResult::OK) {
            OpenModelFromPath(pathBuffer);
        }
    }
    ImGui::SameLine();
    if (DrawToolbarButton(ICON_FA_ARROW_DOWN " Use Selected", HasSelectedEntityContext())) {
        ImportFromSelectedEntity();
    }
    ImGui::SameLine();
    if (DrawToolbarButton(ICON_FA_LINK " Bind Selected", HasSelectedEntityContext())) {
        SetPreviewEntity(m_selectedEntity);
        ImportSocketsFromPreviewEntity();
    }
    ImGui::SameLine();
    if (DrawToolbarButton(ICON_FA_FOLDER_OPEN " Timeline")) {
        char pathBuffer[MAX_PATH] = {};
        if (!m_timelineAssetPath.empty()) {
            strcpy_s(pathBuffer, m_timelineAssetPath.c_str());
        }
        if (Dialog::OpenFileName(pathBuffer, MAX_PATH, kTimelineFileFilter, "Open Timeline") == DialogResult::OK) {
            OpenTimelineFromPath(pathBuffer);
        }
    }
    ImGui::SameLine();
    if (DrawToolbarButton(ICON_FA_FOLDER_OPEN " State")) {
        char pathBuffer[MAX_PATH] = {};
        if (!m_stateMachineAssetPath.empty()) {
            strcpy_s(pathBuffer, m_stateMachineAssetPath.c_str());
        }
        if (Dialog::OpenFileName(pathBuffer, MAX_PATH, kStateMachineFileFilter, "Open State Machine") == DialogResult::OK) {
            OpenStateMachineFromPath(pathBuffer);
        }
    }
    ImGui::SameLine();
    if (DrawToolbarButton(ICON_FA_FOLDER_OPEN " Input")) {
        char pathBuffer[MAX_PATH] = {};
        const std::string& actionMapPath = m_inputMappingTab.GetActionMapPath();
        if (!actionMapPath.empty()) {
            strcpy_s(pathBuffer, actionMapPath.c_str());
        }
        if (Dialog::OpenFileName(pathBuffer, MAX_PATH, kInputMapFileFilter, "Open Input Map") == DialogResult::OK) {
            OpenInputMapFromPath(pathBuffer);
        }
    }
    ImGui::SameLine();
    if (DrawToolbarButton(ICON_FA_CUBES " Save Prefab", CanUsePreviewEntity())) {
        SavePrefabDocument(false);
    }
    ImGui::SameLine();
    DrawToolbarButton(ICON_FA_FLOPPY_DISK " Save All", HasAnyDirtyDocument()) && SaveAllDocuments(false);
    ImGui::SameLine();
    DrawToolbarButton(ICON_FA_FILE_EXPORT " Save As", HasOpenModel() || !m_timelineAsset.tracks.empty() || !m_stateMachineAsset.states.empty() || !m_inputMappingTab.GetActionMapPath().empty()) && SaveAllDocuments(true);
    ImGui::SameLine();
    DrawToolbarButton(ICON_FA_ROTATE_LEFT " Revert", HasAnyDirtyDocument()) && (RevertAllDocuments(), true);
    ImGui::SameLine();
    if (m_previewState.IsActive()) {
        if (DrawToolbarButton(ICON_FA_STOP " Stop Preview")) {
            Suspend();
        }
    } else {
        DrawToolbarButton(ICON_FA_STOP " Stop Preview", false);
    }
}

void PlayerEditorPanel::DrawEmptyState()
{
    ImGui::Spacing();
    ImGui::TextDisabled("No model opened.");
    ImGui::TextWrapped("Open a model first. The current editor only becomes meaningful after a model is resolved.");
    ImGui::Spacing();

    if (ImGui::Button(ICON_FA_FOLDER_OPEN " Open Model...", ImVec2(180.0f, 0.0f))) {
        char pathBuffer[MAX_PATH] = {};
        if (Dialog::OpenFileName(pathBuffer, MAX_PATH, kModelFileFilter, "Open Model") == DialogResult::OK) {
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
// Main Draw  EHost DockSpace
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
    ImGuiID dockId = ImGui::GetID("PlayerEditorDock");
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
//
//  ┌────────────┬──────────────────────┬──────────────────━E
//  ━EStateMachine━E 3D Viewport        ━EProperties       ━E
//  ━E(NodeGraph) ━E (center, large)    ━E(Details)        ━E
//  ━E            ━E                    ├──────────────────┤
//  ━E            ━E                    ━EAnimator / Input ━E
//  ├─────────────┴─────────────────────┴──────────────────┤
//  ━ETimeline  (full width, bottom)                       ━E
//  └──────────────────────────────────────────────────────━E
//

void PlayerEditorPanel::BuildDockLayout(unsigned int dockspaceId)
{
    ImGuiID dockId = dockspaceId;
    ImGui::DockBuilderRemoveNode(dockId);
    ImGui::DockBuilderAddNode(dockId, ImGuiDockNodeFlags_DockSpace);
    ImGui::DockBuilderSetNodeSize(dockId, ImVec2(1500, 850));

    ImGuiID mainId = dockId;

    // 1. Bottom: Timeline (full width, 35% height  Elike UE's Notify/Track panel)
    ImGuiID bottomId;
    ImGui::DockBuilderSplitNode(mainId, ImGuiDir_Down, 0.35f, &bottomId, &mainId);

    // 2. Left: StateMachine (25% width of top area)
    ImGuiID leftId;
    ImGui::DockBuilderSplitNode(mainId, ImGuiDir_Left, 0.46f, &leftId, &mainId);

    // 3. Right: Properties + Animator/Input (22% width)
    ImGuiID rightId;
    ImGui::DockBuilderSplitNode(mainId, ImGuiDir_Right, 0.22f, &rightId, &mainId);

    // 4. Split right column: Properties top (60%), Animator/Input bottom (40%)
    ImGuiID rightBottomId;
    ImGui::DockBuilderSplitNode(rightId, ImGuiDir_Down, 0.40f, &rightBottomId, &rightId);

    // mainId = center = Viewport (largest area, like UE)
    ImGui::DockBuilderDockWindow(kPEViewportTitle,     mainId);
    ImGui::DockBuilderDockWindow(kPESkeletonTitle,     leftId);     // tabbed left
    ImGui::DockBuilderDockWindow(kPEStateMachineTitle, leftId);     // tabbed left
    ImGui::DockBuilderDockWindow(kPEPropertiesTitle,   rightId);
    ImGui::DockBuilderDockWindow(kPEAnimatorTitle,     rightBottomId);
    ImGui::DockBuilderDockWindow(kPEInputTitle,        rightBottomId); // tabbed
    ImGui::DockBuilderDockWindow(kPETimelineTitle,     bottomId);

    ImGui::DockBuilderFinish(dockId);
}

// ############################################################################
//  VIEWPORT PANEL (3D Model Preview)
// ############################################################################

void PlayerEditorPanel::DrawViewportPanel()
{
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    const bool open = ImGui::Begin(kPEViewportTitle);
    ImGui::PopStyleVar();
    if (!open) { ImGui::End(); return; }

    const float sharedDirLengthSq =
        m_sharedSceneCameraDirection.x * m_sharedSceneCameraDirection.x +
        m_sharedSceneCameraDirection.y * m_sharedSceneCameraDirection.y +
        m_sharedSceneCameraDirection.z * m_sharedSceneCameraDirection.z;
    const bool usingSharedSceneView = sharedDirLengthSq > 0.0001f;

    const float toolbarTop = 4.0f;
    const float toolbarLeft = 6.0f;
    const float toolbarHeight = ImGui::GetFrameHeightWithSpacing() + 8.0f;

    float contentTop = 0.0f;
    if (!usingSharedSceneView) {
        ImGui::SetCursorPos(ImVec2(toolbarLeft, toolbarTop));
        ImGui::SetNextItemWidth(180.0f);
        ImGui::SliderFloat("Preview Scale", &m_previewModelScale, 0.01f, 5.00f, "%.2f");
        ImGui::SameLine();
        if (ImGui::Button("Reset Scale")) {
            m_previewModelScale = 1.0f;
        }
        contentTop = toolbarHeight;
    }

    ImGui::SetCursorPos(ImVec2(0.0f, contentTop));
    ImVec2 avail = ImGui::GetContentRegionAvail();
    m_previewRenderSize = {
        (std::max)(avail.x, 0.0f),
        (std::max)(avail.y, 0.0f)
    };
    m_viewportHovered = false;
    m_viewportRect = { 0.0f, 0.0f, avail.x, avail.y };

    if (!HasOpenModel() && !m_viewportTexture) {
        ImGui::SetCursorPos(ImVec2(12.0f, toolbarHeight + 12.0f));
        DrawEmptyState();
        if (ImGui::BeginDragDropTarget()) {
            if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ENGINE_ASSET")) {
                const std::string droppedPath(static_cast<const char*>(payload->Data));
                if (HasExtension(droppedPath, { ".fbx", ".gltf", ".glb", ".obj" })) {
                    OpenModelFromPath(droppedPath);
                }
            }
            ImGui::EndDragDropTarget();
        }
        ImGui::End();
        return;
    }

    if (m_viewportTexture) {
        // Display the dedicated render target
        void* texId = ImGuiRenderer::GetTextureID(m_viewportTexture);
        if (texId) {
            ImGui::Image((ImTextureID)texId, avail);

            const ImVec2 imageMin = ImGui::GetItemRectMin();
            const ImVec2 imageMax = ImGui::GetItemRectMax();
            const ImVec2 imageSize = ImVec2(imageMax.x - imageMin.x, imageMax.y - imageMin.y);
            m_viewportRect = { imageMin.x, imageMin.y, imageSize.x, imageSize.y };
            m_viewportHovered = ImGui::IsItemHovered();
            const int markerBoneIndex = (m_hoveredBoneIndex >= 0) ? m_hoveredBoneIndex : m_selectedBoneIndex;
            if (m_model && markerBoneIndex >= 0) {
                ImVec2 markerPos{};
                if (ProjectBoneMarkerToViewport(
                    m_model,
                    markerBoneIndex,
                    usingSharedSceneView ? 1.0f : m_previewModelScale,
                    GetPreviewCameraPosition(),
                    GetPreviewCameraTarget(),
                    GetPreviewCameraFovY(),
                    GetPreviewNearZ(),
                    GetPreviewFarZ(),
                    imageMin,
                    imageSize,
                    markerPos))
                {
                    const auto& nodes = m_model->GetNodes();
                    const char* boneName = (markerBoneIndex >= 0 && markerBoneIndex < static_cast<int>(nodes.size()))
                        ? nodes[markerBoneIndex].name.c_str()
                        : "";

                    ImDrawList* dl = ImGui::GetWindowDrawList();
                    dl->PushClipRect(imageMin, imageMax, true);
                    dl->AddCircleFilled(markerPos, 5.0f, IM_COL32(255, 230, 80, 255));
                    dl->AddCircle(markerPos, 9.0f, IM_COL32(255, 255, 255, 220), 0, 1.5f);
                    dl->AddLine(ImVec2(markerPos.x - 10.0f, markerPos.y), ImVec2(markerPos.x - 3.0f, markerPos.y), IM_COL32(255, 255, 255, 220), 2.0f);
                    dl->AddLine(ImVec2(markerPos.x + 3.0f, markerPos.y), ImVec2(markerPos.x + 10.0f, markerPos.y), IM_COL32(255, 255, 255, 220), 2.0f);
                    dl->AddLine(ImVec2(markerPos.x, markerPos.y - 10.0f), ImVec2(markerPos.x, markerPos.y - 3.0f), IM_COL32(255, 255, 255, 220), 2.0f);
                    dl->AddLine(ImVec2(markerPos.x, markerPos.y + 3.0f), ImVec2(markerPos.x, markerPos.y + 10.0f), IM_COL32(255, 255, 255, 220), 2.0f);
                    if (boneName && boneName[0] != '\0') {
                        ImVec2 labelSize = ImGui::CalcTextSize(boneName);
                        const float margin = 6.0f;
                        ImVec2 labelPos(markerPos.x + 12.0f, markerPos.y - labelSize.y - 8.0f);
                        if (labelPos.x + labelSize.x + 8.0f > imageMax.x - margin) {
                            labelPos.x = markerPos.x - labelSize.x - 16.0f;
                        }
                        if (labelPos.x < imageMin.x + margin) {
                            labelPos.x = imageMin.x + margin;
                        }
                        if (labelPos.y < imageMin.y + margin) {
                            labelPos.y = markerPos.y + 12.0f;
                        }
                        if (labelPos.y + labelSize.y + 4.0f > imageMax.y - margin) {
                            labelPos.y = imageMax.y - labelSize.y - 4.0f - margin;
                        }
                        dl->AddRectFilled(
                            ImVec2(labelPos.x - 4.0f, labelPos.y - 2.0f),
                            ImVec2(labelPos.x + labelSize.x + 4.0f, labelPos.y + labelSize.y + 2.0f),
                            IM_COL32(20, 20, 20, 220),
                            4.0f);
                        dl->AddText(labelPos, IM_COL32(255, 255, 255, 240), boneName);
                    }
                    dl->PopClipRect();
                }
            }
        }
    } else {
        // Placeholder: dark background with instructions
        ImDrawList* dl = ImGui::GetWindowDrawList();
        ImVec2 pos = ImGui::GetCursorScreenPos();
        m_viewportRect = { pos.x, pos.y, avail.x, avail.y };
        dl->AddRectFilled(pos, ImVec2(pos.x + avail.x, pos.y + avail.y), IM_COL32(20, 20, 25, 255));

        // Center text
        const char* msg = "3D Model Viewport";
        ImVec2 textSize = ImGui::CalcTextSize(msg);
        dl->AddText(ImVec2(pos.x + (avail.x - textSize.x) * 0.5f, pos.y + avail.y * 0.4f),
            IM_COL32(120, 120, 120, 255), msg);

        const char* sub = "Set viewport texture via SetViewportTexture()";
        ImVec2 subSize = ImGui::CalcTextSize(sub);
        dl->AddText(ImVec2(pos.x + (avail.x - subSize.x) * 0.5f, pos.y + avail.y * 0.4f + 20),
            IM_COL32(80, 80, 80, 255), sub);

        ImGui::Dummy(avail);
    }

    // Orbit camera controls (mouse drag on viewport)
    if (ImGui::IsItemHovered() && !usingSharedSceneView && !m_viewportTexture) {
        // Right-drag: orbit
        if (ImGui::IsMouseDragging(1)) {
            ImVec2 delta = ImGui::GetMouseDragDelta(1);
            m_vpCameraYaw   += delta.x * 0.005f;
            m_vpCameraPitch += delta.y * 0.005f;
            m_vpCameraPitch = (std::max)(-1.5f, (std::min)(1.5f, m_vpCameraPitch));
            ImGui::ResetMouseDragDelta(1);
        }
        // Scroll: zoom
        float wheel = ImGui::GetIO().MouseWheel;
        if (wheel != 0.0f) {
            m_vpCameraDist -= wheel * 0.5f;
            m_vpCameraDist = (std::max)(0.5f, (std::min)(50.0f, m_vpCameraDist));
        }
    }

    ImGui::End();
}

DirectX::XMFLOAT3 PlayerEditorPanel::GetPreviewCameraTarget() const
{
    const float dirLengthSq =
        m_sharedSceneCameraDirection.x * m_sharedSceneCameraDirection.x +
        m_sharedSceneCameraDirection.y * m_sharedSceneCameraDirection.y +
        m_sharedSceneCameraDirection.z * m_sharedSceneCameraDirection.z;
    if (dirLengthSq > 0.0001f) {
        return {
            m_sharedSceneCameraPosition.x + m_sharedSceneCameraDirection.x,
            m_sharedSceneCameraPosition.y + m_sharedSceneCameraDirection.y,
            m_sharedSceneCameraPosition.z + m_sharedSceneCameraDirection.z
        };
    }

    if (m_model) {
        const auto bounds = m_model->GetWorldBounds();
        DirectX::XMFLOAT3 target = bounds.Center;
        target.y += bounds.Extents.y * 0.12f;
        return target;
    }

    if (m_registry && !Entity::IsNull(m_previewEntity)) {
        if (const auto* transform = m_registry->GetComponent<TransformComponent>(m_previewEntity)) {
            return transform->worldPosition;
        }
    }

    return { 0.0f, 1.0f, 0.0f };
}

DirectX::XMFLOAT3 PlayerEditorPanel::GetPreviewCameraDirection() const
{
    const float sharedDirLengthSq =
        m_sharedSceneCameraDirection.x * m_sharedSceneCameraDirection.x +
        m_sharedSceneCameraDirection.y * m_sharedSceneCameraDirection.y +
        m_sharedSceneCameraDirection.z * m_sharedSceneCameraDirection.z;
    if (sharedDirLengthSq > 0.0001f) {
        return m_sharedSceneCameraDirection;
    }

    const float cosPitch = std::cos(m_vpCameraPitch);
    DirectX::XMFLOAT3 dir = {
        std::sin(m_vpCameraYaw) * cosPitch,
        std::sin(m_vpCameraPitch),
        std::cos(m_vpCameraYaw) * cosPitch
    };

    const float lenSq = dir.x * dir.x + dir.y * dir.y + dir.z * dir.z;
    if (lenSq > 0.0001f) {
        const float invLen = 1.0f / std::sqrt(lenSq);
        dir.x *= invLen;
        dir.y *= invLen;
        dir.z *= invLen;
    } else {
        dir = { 0.0f, 0.0f, 1.0f };
    }
    return dir;
}

DirectX::XMFLOAT3 PlayerEditorPanel::GetPreviewCameraPosition() const
{
    const float sharedDirLengthSq =
        m_sharedSceneCameraDirection.x * m_sharedSceneCameraDirection.x +
        m_sharedSceneCameraDirection.y * m_sharedSceneCameraDirection.y +
        m_sharedSceneCameraDirection.z * m_sharedSceneCameraDirection.z;
    if (sharedDirLengthSq > 0.0001f) {
        return m_sharedSceneCameraPosition;
    }

    const DirectX::XMFLOAT3 target = GetPreviewCameraTarget();
    const DirectX::XMFLOAT3 dir = GetPreviewCameraDirection();
    return {
        target.x - dir.x * m_vpCameraDist,
        target.y - dir.y * m_vpCameraDist,
        target.z - dir.z * m_vpCameraDist
    };
}

float PlayerEditorPanel::GetPreviewCameraFovY() const
{
    const float sharedDirLengthSq =
        m_sharedSceneCameraDirection.x * m_sharedSceneCameraDirection.x +
        m_sharedSceneCameraDirection.y * m_sharedSceneCameraDirection.y +
        m_sharedSceneCameraDirection.z * m_sharedSceneCameraDirection.z;
    if (sharedDirLengthSq > 0.0001f) {
        return m_sharedSceneCameraFovY;
    }
    return 0.785398f;
}

// ############################################################################
//  MODEL SETTER
// ############################################################################

void PlayerEditorPanel::SetModel(const Model* model)
{
    if (m_model == model) return;
    Suspend();
    m_ownedModel.reset();
    m_model = model;
    ResetSelectionState();
}

// ############################################################################
//  SKELETON PANEL  EBone Tree + Socket Management
// ############################################################################

void PlayerEditorPanel::DrawSkeletonPanel()
{
    if (!ImGui::Begin(kPESkeletonTitle)) { ImGui::End(); return; }

    m_hoveredBoneIndex = -1;

    if (!m_model) {
        ImGui::TextDisabled("No model assigned.");
        ImGui::End();
        return;
    }

    const auto& nodes = m_model->GetNodes();

    ImGui::SetNextItemWidth(-1);
    ImGui::InputTextWithHint("##BoneSearch", ICON_FA_MAGNIFYING_GLASS " Search bones...",
        m_boneSearchFilter, sizeof(m_boneSearchFilter));

    ImGui::Separator();

    const float socketPaneHeight = 28.0f;
    const float treePaneHeight = (std::max)(0.0f, ImGui::GetContentRegionAvail().y - socketPaneHeight - ImGui::GetStyle().ItemSpacing.y);
    ImGui::BeginChild("BoneTree", ImVec2(0, treePaneHeight), false, ImGuiWindowFlags_HorizontalScrollbar);

    if (m_boneSearchFilter[0] == '\0') {
        for (int i = 0; i < (int)nodes.size(); ++i) {
            if (nodes[i].parentIndex < 0) {
                DrawBoneTreeNode(i);
            }
        }
    } else {
        std::string filterLower(m_boneSearchFilter);
        for (auto& c : filterLower) c = (char)tolower(c);

        for (int i = 0; i < (int)nodes.size(); ++i) {
            std::string nameLower = nodes[i].name;
            for (auto& c : nameLower) c = (char)tolower(c);

            if (nameLower.find(filterLower) != std::string::npos) {
                bool selected = (m_selectedBoneIndex == i);
                if (ImGui::Selectable(("[" + std::to_string(i) + "] " + nodes[i].name).c_str(), selected)) {
                    m_selectedBoneIndex = i;
                    m_selectedBoneName = nodes[i].name;
                    m_selectionCtx = SelectionContext::Bone;
                }
            }
        }
    }

    ImGui::EndChild();

    ImGui::Dummy(ImVec2(0, ImGui::GetStyle().ItemSpacing.y * 0.1f));
    DrawSocketList(socketPaneHeight);

    ImGui::End();
}
void PlayerEditorPanel::DrawBoneTreeNode(int nodeIndex)
{
    if (!m_model) return;
    const auto& nodes = m_model->GetNodes();
    if (nodeIndex < 0 || nodeIndex >= (int)nodes.size()) return;

    const auto& node = nodes[nodeIndex];
    bool hasChildren = !node.children.empty();
    bool selected = (m_selectedBoneIndex == nodeIndex);

    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth;
    if (selected) flags |= ImGuiTreeNodeFlags_Selected;
    if (!hasChildren) flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;

    // Icon: bone vs joint
    const char* icon = hasChildren ? ICON_FA_BONE : ICON_FA_CIRCLE;
    std::string label = std::string(icon) + " " + node.name;

    bool open = ImGui::TreeNodeEx(("##bone_" + std::to_string(nodeIndex)).c_str(), flags, "%s", label.c_str());

    if (ImGui::IsItemHovered()) {
        m_hoveredBoneIndex = nodeIndex;
    }

    // Click to select
    if (ImGui::IsItemClicked(0)) {
        m_selectedBoneIndex = nodeIndex;
        m_selectedBoneName = node.name;
        m_selectionCtx = SelectionContext::Bone;
    }

    // Right-click: quick actions
    if (ImGui::IsItemClicked(1)) {
        m_selectedBoneIndex = nodeIndex;
        m_selectedBoneName = node.name;
        ImGui::OpenPopup("BoneCtx");
    }

    if (ImGui::BeginPopup("BoneCtx")) {
        ImGui::Text(ICON_FA_BONE " %s [%d]", m_selectedBoneName.c_str(), m_selectedBoneIndex);
        ImGui::Separator();
        if (ImGui::MenuItem(ICON_FA_CROSSHAIRS " Set as Hitbox Node")) {
            for (auto& track : m_timelineAsset.tracks) {
                if ((int)track.id != m_selectedTrackId) continue;
                if (m_selectedItemIdx < 0 || m_selectedItemIdx >= (int)track.items.size()) break;
                if (track.type == TimelineTrackType::Hitbox)
                    track.items[m_selectedItemIdx].hitbox.nodeIndex = m_selectedBoneIndex;
                m_timelineDirty = true;
            }
        }
        if (ImGui::MenuItem(ICON_FA_WAND_MAGIC_SPARKLES " Set as VFX Node")) {
            for (auto& track : m_timelineAsset.tracks) {
                if ((int)track.id != m_selectedTrackId) continue;
                if (m_selectedItemIdx < 0 || m_selectedItemIdx >= (int)track.items.size()) break;
                if (track.type == TimelineTrackType::VFX)
                    track.items[m_selectedItemIdx].vfx.nodeIndex = m_selectedBoneIndex;
                m_timelineDirty = true;
            }
        }
        if (ImGui::MenuItem(ICON_FA_PLUG " Create Socket Here")) {
            NodeSocket sock;
            sock.name = "Socket_" + node.name;
            sock.parentBoneName = node.name;
            sock.cachedBoneIndex = nodeIndex;
            m_sockets.push_back(sock);
            m_socketDirty = true;
        }
        ImGui::EndPopup();
    }

    // Recurse children
    if (open && hasChildren) {
        for (auto* child : node.children) {
            // Find child index
            int childIdx = (int)(child - &nodes[0]);
            DrawBoneTreeNode(childIdx);
        }
        ImGui::TreePop();
    }
}

void PlayerEditorPanel::DrawSocketList(float height)
{
    ImGui::BeginChild("SocketList", ImVec2(0, height), false, ImGuiWindowFlags_HorizontalScrollbar);
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(3.0f, 1.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(2.0f, 1.0f));

    ImGui::TextDisabled(ICON_FA_PLUG " Sockets (%d)", (int)m_sockets.size());
    ImGui::SameLine();
    if (ImGui::SmallButton(ICON_FA_PLUS " Add")) {
        NodeSocket sock;
        sock.name = "NewSocket";
        if (m_selectedBoneIndex >= 0 && !m_selectedBoneName.empty()) {
            sock.parentBoneName = m_selectedBoneName;
            sock.cachedBoneIndex = m_selectedBoneIndex;
            sock.name = "Socket_" + m_selectedBoneName;
        }
        m_sockets.push_back(sock);
        m_socketDirty = true;
    }

    ImGui::Separator();

    for (int si = 0; si < (int)m_sockets.size(); ++si) {
        auto& sock = m_sockets[si];
        ImGui::PushID(si);

        bool selected = (m_selectedSocketIdx == si);
        std::string label = sock.name + " -> " + sock.parentBoneName;
        if (ImGui::Selectable(label.c_str(), selected)) {
            m_selectedSocketIdx = si;
            m_selectionCtx = SelectionContext::Socket;
        }

        ImGui::PopID();
    }

    ImGui::PopStyleVar(2);
    ImGui::EndChild();
}

// ############################################################################
//  STATE MACHINE PANEL
// ############################################################################

void PlayerEditorPanel::DrawStateMachinePanel()
{
    if (!ImGui::Begin(kPEStateMachineTitle)) { ImGui::End(); return; }

    const float listWidth = 260.0f;
    const bool hasSelectedState = m_stateMachineAsset.FindState(m_selectedNodeId) != nullptr;

    if (ImGui::Button(ICON_FA_FLOPPY_DISK " Save")) {
        SaveStateMachineDocument(false);
    }
    ImGui::SameLine();
    if (ImGui::Button(ICON_FA_FILE_EXPORT " Save As")) {
        SaveStateMachineDocument(true);
    }
    ImGui::SameLine();
    if (ImGui::Button(ICON_FA_PLUS " Add State")) {
        ImGui::OpenPopup("AddStateTemplatePopup");
    }
    ImGui::SameLine();
    if (DrawToolbarButton(ICON_FA_PLAY " Preview State", hasSelectedState)) {
        PreviewStateNode(m_selectedNodeId, true);
    }
    ImGui::SameLine();
    if (ImGui::Button(ICON_FA_ARROWS_TO_CIRCLE " Fit")) {
        m_graphOffset = { 200, 150 };
        m_graphZoom = 1.0f;
    }
    ImGui::SameLine();
    ImGui::SetNextItemWidth(80);
    ImGui::SliderFloat("Zoom##SM", &m_graphZoom, 0.3f, 3.0f, "%.1f");
    ImGui::SameLine();
    if (hasSelectedState && ImGui::Button(ICON_FA_STAR " Default")) {
        m_stateMachineAsset.defaultStateId = m_selectedNodeId;
        m_stateMachineDirty = true;
    }

    if (m_isConnecting) {
        ImGui::TextColored(ImVec4(1, 0.8f, 0.2f, 1), ICON_FA_LINK " Connecting... (ESC cancel)");
        if (ImGui::IsKeyPressed(ImGuiKey_Escape)) {
            m_isConnecting = false;
            m_connectFromNodeId = 0;
        }
    }

    if (ImGui::BeginPopup("AddStateTemplatePopup")) {
        if (ImGui::MenuItem("Locomotion")) AddStateTemplate(StateNodeType::Locomotion, { 0.0f, 0.0f });
        if (ImGui::MenuItem("Action"))     AddStateTemplate(StateNodeType::Action, { 0.0f, 0.0f });
        if (ImGui::MenuItem("Dodge"))      AddStateTemplate(StateNodeType::Dodge, { 0.0f, 0.0f });
        if (ImGui::MenuItem("Jump"))       AddStateTemplate(StateNodeType::Jump, { 0.0f, 0.0f });
        if (ImGui::MenuItem("Damage"))     AddStateTemplate(StateNodeType::Damage, { 0.0f, 0.0f });
        if (ImGui::MenuItem("Dead"))       AddStateTemplate(StateNodeType::Dead, { 0.0f, 0.0f });
        if (ImGui::MenuItem("Custom"))     AddStateTemplate(StateNodeType::Custom, { 0.0f, 0.0f });
        ImGui::EndPopup();
    }

    if (DrawDocumentPathLabel("Document", m_stateMachineAssetPath, m_stateMachineDirty)) {
        ImGui::Separator();
    }

    ImGui::BeginChild("StateListPane", ImVec2(listWidth, 0.0f), ImGuiChildFlags_Borders);
    ImGui::Text(ICON_FA_LIST " States");
    ImGui::Separator();
    for (const auto& state : m_stateMachineAsset.states) {
        const bool selected = (m_selectedNodeId == state.id);
        std::string label = state.name + "##state_list_" + std::to_string(state.id);
        if (ImGui::Selectable(label.c_str(), selected)) {
            m_selectedNodeId = state.id;
            m_selectedTransitionId = 0;
            m_selectionCtx = SelectionContext::StateNode;
        }
        if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0)) {
            PreviewStateNode(state.id, true);
        }

        ImGui::Indent();
        ImGui::TextDisabled("%s", GetStateTypeLabel(state.type));
        std::string animLabel = "Animation: ";
        if (m_model && state.animationIndex >= 0 && state.animationIndex < static_cast<int>(m_model->GetAnimations().size())) {
            animLabel += m_model->GetAnimations()[state.animationIndex].name;
        } else {
            animLabel += "(none)";
        }
        ImGui::TextDisabled("%s", animLabel.c_str());
        const std::string timelineLabel = std::string("Timeline: ") + (state.timelineAssetPath.empty() ? "(none)" : std::filesystem::path(state.timelineAssetPath).filename().string());
        ImGui::TextDisabled("%s", timelineLabel.c_str());
        ImGui::TextDisabled("Transitions: %d%s", static_cast<int>(m_stateMachineAsset.GetTransitionsFrom(state.id).size()), m_stateMachineAsset.defaultStateId == state.id ? "  DEFAULT" : "");
        ImGui::Unindent();
        ImGui::Separator();
    }
    ImGui::EndChild();

    ImGui::SameLine();
    ImGui::BeginGroup();
    ImGui::TextDisabled("Double-click state to preview. Right-click graph to add states.");
    ImVec2 canvasSize = ImGui::GetContentRegionAvail();
    DrawNodeGraph(canvasSize);
    ImGui::EndGroup();

    ImGui::End();
}

void PlayerEditorPanel::DrawNodeGraph(ImVec2 canvasSize)
{
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 origin = ImGui::GetCursorScreenPos();

    // Background
    dl->AddRectFilled(origin, ImVec2(origin.x + canvasSize.x, origin.y + canvasSize.y),
        IM_COL32(22, 22, 28, 255));

    // Grid
    float gridStep = 32.0f * m_graphZoom;
    if (gridStep > 4.0f) {
        for (float x = fmodf(m_graphOffset.x, gridStep); x < canvasSize.x; x += gridStep)
            dl->AddLine(ImVec2(origin.x + x, origin.y), ImVec2(origin.x + x, origin.y + canvasSize.y),
                IM_COL32(40, 40, 48, 255));
        for (float y = fmodf(m_graphOffset.y, gridStep); y < canvasSize.y; y += gridStep)
            dl->AddLine(ImVec2(origin.x, origin.y + y), ImVec2(origin.x + canvasSize.x, origin.y + y),
                IM_COL32(40, 40, 48, 255));
    }

    auto NodeScreenPos = [&](const StateNode& s) -> ImVec2 {
        return ImVec2(
            origin.x + m_graphOffset.x + s.position.x * m_graphZoom,
            origin.y + m_graphOffset.y + s.position.y * m_graphZoom);
    };
    auto NodeScreenCenter = [&](const StateNode& s) -> ImVec2 {
        auto p = NodeScreenPos(s);
        return ImVec2(p.x + kNodeWidth * 0.5f * m_graphZoom, p.y + kNodeHeight * 0.5f * m_graphZoom);
    };

    // ── Draw transitions (arrows) ──
    for (auto& trans : m_stateMachineAsset.transitions) {
        auto* from = m_stateMachineAsset.FindState(trans.fromState);
        auto* to   = m_stateMachineAsset.FindState(trans.toState);
        if (!from || !to) continue;

        ImVec2 p1 = NodeScreenCenter(*from);
        ImVec2 p2 = NodeScreenCenter(*to);

        bool isSel = (m_selectedTransitionId == trans.id);
        ImU32 lineCol = isSel ? IM_COL32(255, 255, 80, 255) : IM_COL32(180, 180, 180, 160);
        float thickness = isSel ? 3.0f : 2.0f;
        dl->AddLine(p1, p2, lineCol, thickness);

        // Arrowhead at midpoint
        ImVec2 dir(p2.x - p1.x, p2.y - p1.y);
        float len = sqrtf(dir.x * dir.x + dir.y * dir.y);
        if (len > 1.0f) {
            dir.x /= len; dir.y /= len;
            ImVec2 mid((p1.x + p2.x) * 0.5f, (p1.y + p2.y) * 0.5f);
            ImVec2 perp(-dir.y, dir.x);
            float as = 10.0f;
            dl->AddTriangleFilled(
                ImVec2(mid.x + dir.x * as, mid.y + dir.y * as),
                ImVec2(mid.x - dir.x * as * 0.6f + perp.x * as * 0.5f, mid.y - dir.y * as * 0.6f + perp.y * as * 0.5f),
                ImVec2(mid.x - dir.x * as * 0.6f - perp.x * as * 0.5f, mid.y - dir.y * as * 0.6f - perp.y * as * 0.5f),
                lineCol);

            // Click on midpoint to select transition
            ImVec2 hitMin(mid.x - 12, mid.y - 12);
            ImVec2 hitMax(mid.x + 12, mid.y + 12);
            ImGui::SetCursorScreenPos(hitMin);
            ImGui::InvisibleButton(("trans_" + std::to_string(trans.id)).c_str(), ImVec2(24, 24));
            if (ImGui::IsItemClicked(0)) {
                m_selectedTransitionId = trans.id;
                m_selectedNodeId = 0;
                m_selectionCtx = SelectionContext::Transition;
            }
        }

        // Condition count badge
        if (!trans.conditions.empty()) {
            ImVec2 mid((p1.x + p2.x) * 0.5f, (p1.y + p2.y) * 0.5f);
            char badge[8];
            snprintf(badge, sizeof(badge), "%d", (int)trans.conditions.size());
            dl->AddText(ImVec2(mid.x + 14, mid.y - 14), IM_COL32(200, 200, 100, 255), badge);
        }
    }

    // ── Draw connection wire (while connecting) ──
    if (m_isConnecting) {
        auto* from = m_stateMachineAsset.FindState(m_connectFromNodeId);
        if (from) {
            ImVec2 p1 = NodeScreenCenter(*from);
            ImVec2 p2 = ImGui::GetMousePos();
            dl->AddLine(p1, p2, IM_COL32(255, 200, 50, 200), 2.0f);
        }
    }

    // ── Draw state nodes ──
    for (auto& state : m_stateMachineAsset.states) {
        ImVec2 nPos = NodeScreenPos(state);
        ImVec2 nSize(kNodeWidth * m_graphZoom, kNodeHeight * m_graphZoom);
        ImVec2 nEnd(nPos.x + nSize.x, nPos.y + nSize.y);

        bool isSel     = (m_selectedNodeId == state.id);
        bool isDefault = (m_stateMachineAsset.defaultStateId == state.id);

        // Shadow
        dl->AddRectFilled(ImVec2(nPos.x + 3, nPos.y + 3), ImVec2(nEnd.x + 3, nEnd.y + 3),
            IM_COL32(0, 0, 0, 80), 8.0f);

        // Body
        ImU32 nodeCol = StateNodeColor(state.type);
        dl->AddRectFilled(nPos, nEnd, nodeCol, 8.0f);

        // Selection / default border
        if (isSel) dl->AddRect(nPos, nEnd, IM_COL32(255, 255, 255, 255), 8.0f, 0, 3.0f);
        if (isDefault) dl->AddRect(
            ImVec2(nPos.x - 2, nPos.y - 2), ImVec2(nEnd.x + 2, nEnd.y + 2),
            IM_COL32(255, 200, 50, 200), 10.0f, 0, 2.0f);

        // Label
        float fontSize = ImGui::GetFontSize();
        ImVec2 textPos(nPos.x + 8 * m_graphZoom, nPos.y + nSize.y * 0.5f - fontSize * 0.5f);
        dl->AddText(textPos, IM_COL32(255, 255, 255, 255), state.name.c_str());

        // Type badge (small text)
        const char* typeLabels[] = { "LOCO", "ACT", "DODGE", "JUMP", "DMG", "DEAD", "CUSTOM" };
        int ti = (int)state.type;
        if (ti >= 0 && ti < 7) {
            ImVec2 badgePos(nEnd.x - 40 * m_graphZoom, nPos.y + 2 * m_graphZoom);
            dl->AddText(badgePos, IM_COL32(255, 255, 255, 120), typeLabels[ti]);
        }

        // Interaction
        ImGui::SetCursorScreenPos(nPos);
        ImGui::InvisibleButton(("node_" + std::to_string(state.id)).c_str(), nSize);

        if (ImGui::IsItemClicked(0)) {
            if (m_isConnecting) {
                // Finish connection
                if (m_connectFromNodeId != state.id) {
                    m_stateMachineAsset.AddTransition(m_connectFromNodeId, state.id);
                    m_stateMachineDirty = true;
                }
                m_isConnecting = false;
                m_connectFromNodeId = 0;
            } else {
                m_selectedNodeId = state.id;
                m_selectedTransitionId = 0;
                m_selectionCtx = SelectionContext::StateNode;
            }
        }

        // Double-click: open timeline for this state
        if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0)) {
            if (!state.timelineAssetPath.empty()) {
                OpenTimelineFromPath(state.timelineAssetPath);
            }
        }

        // Drag node
        if (ImGui::IsItemActive() && ImGui::IsMouseDragging(0) && !m_isConnecting) {
            ImVec2 delta = ImGui::GetMouseDragDelta(0);
            state.position.x += delta.x / m_graphZoom;
            state.position.y += delta.y / m_graphZoom;
            m_stateMachineDirty = true;
            ImGui::ResetMouseDragDelta();
        }

        // Right-click context
        if (ImGui::IsItemClicked(1)) {
            m_selectedNodeId = state.id;
            m_selectionCtx = SelectionContext::StateNode;
            ImGui::OpenPopup("NodeCtx");
        }
    }

    // ── Node context menu ──
    if (ImGui::BeginPopup("NodeCtx")) {
        if (ImGui::MenuItem(ICON_FA_STAR " Set Default")) {
            m_stateMachineAsset.defaultStateId = m_selectedNodeId;
            m_stateMachineDirty = true;
        }
        if (ImGui::MenuItem(ICON_FA_LINK " Connect From Here...")) {
            m_isConnecting = true;
            m_connectFromNodeId = m_selectedNodeId;
        }
        ImGui::Separator();
        if (ImGui::MenuItem(ICON_FA_TRASH " Delete State")) {
            m_stateMachineAsset.RemoveState(m_selectedNodeId);
            m_selectedNodeId = 0;
            m_selectionCtx = SelectionContext::None;
            m_stateMachineDirty = true;
        }
        ImGui::EndPopup();
    }

    // ── Background interaction ──
    ImGui::SetCursorScreenPos(origin);
    ImGui::InvisibleButton("graph_bg", canvasSize);

    // Pan with middle-mouse or right-drag on empty
    if (ImGui::IsItemActive() && ImGui::IsMouseDragging(2)) {
        ImVec2 d = ImGui::GetMouseDragDelta(2);
        m_graphOffset.x += d.x;
        m_graphOffset.y += d.y;
        ImGui::ResetMouseDragDelta(2);
    }

    // Zoom with scroll wheel
    if (ImGui::IsItemHovered()) {
        float wheel = ImGui::GetIO().MouseWheel;
        if (wheel != 0.0f) {
            m_graphZoom *= (wheel > 0) ? 1.1f : 0.9f;
            m_graphZoom = (std::max)(0.2f, (std::min)(4.0f, m_graphZoom));
        }
    }

    // Right-click on background: add state
    if (ImGui::IsItemClicked(1)) {
        ImGui::OpenPopup("GraphBgCtx");
    }

    if (ImGui::BeginPopup("GraphBgCtx")) {
        ImVec2 mousePos = ImGui::GetMousePosOnOpeningCurrentPopup();
        float posX = (mousePos.x - origin.x - m_graphOffset.x) / m_graphZoom;
        float posY = (mousePos.y - origin.y - m_graphOffset.y) / m_graphZoom;

        ImGui::TextDisabled("Add State:");
        if (ImGui::MenuItem("Locomotion")) AddStateTemplate(StateNodeType::Locomotion, { posX, posY });
        if (ImGui::MenuItem("Action"))     AddStateTemplate(StateNodeType::Action, { posX, posY });
        if (ImGui::MenuItem("Dodge"))      AddStateTemplate(StateNodeType::Dodge, { posX, posY });
        if (ImGui::MenuItem("Jump"))       AddStateTemplate(StateNodeType::Jump, { posX, posY });
        if (ImGui::MenuItem("Damage"))     AddStateTemplate(StateNodeType::Damage, { posX, posY });
        if (ImGui::MenuItem("Dead"))       AddStateTemplate(StateNodeType::Dead, { posX, posY });
        if (ImGui::MenuItem("Custom"))     AddStateTemplate(StateNodeType::Custom, { posX, posY });
        ImGui::EndPopup();
    }
}

// ############################################################################
//  TIMELINE PANEL
// ############################################################################

void PlayerEditorPanel::DrawTimelinePanel()
{
    if (!ImGui::Begin(kPETimelineTitle)) { ImGui::End(); return; }

    DrawTimelinePlaybackToolbar();

    float availH = ImGui::GetContentRegionAvail().y;
    float availW = ImGui::GetContentRegionAvail().x;

    // Left: Track Headers
    ImGui::BeginChild("TLHeaders", ImVec2(kTrackHeaderWidth, availH), ImGuiChildFlags_Borders);
    DrawTimelineTrackHeaders(availH);
    ImGui::EndChild();

    ImGui::SameLine();

    // Right: Grid
    ImGui::BeginChild("TLGrid", ImVec2(availW - kTrackHeaderWidth - 8, availH),
        ImGuiChildFlags_Borders, ImGuiWindowFlags_HorizontalScrollbar);
    DrawTimelineGrid(availH);
    ImGui::EndChild();

    ImGui::End();
}

void PlayerEditorPanel::DrawTimelinePlaybackToolbar()
{
    const float fps = m_timelineAsset.fps > 0.0f ? m_timelineAsset.fps : 60.0f;
    const float durationSeconds = GetSelectedAnimationDurationSeconds();

    if (m_isPlaying && m_previewState.IsActive()) {
        m_previewState.AdvanceTime(ImGui::GetIO().DeltaTime, m_timelineAsset);
        if (durationSeconds > 0.0f) {
            float previewTime = m_previewState.GetDriver()->GetTime();
            if (m_previewState.GetDriver()->IsLoop()) {
                if (previewTime > durationSeconds) {
                    previewTime = std::fmod(previewTime, durationSeconds);
                    m_previewState.SetTime(previewTime);
                }
            } else if (previewTime > durationSeconds) {
                previewTime = durationSeconds;
                m_previewState.SetTime(previewTime);
            }
        }
        m_playheadFrame = m_previewState.GetCurrentFrame(fps);
        if (durationSeconds > 0.0f) {
            const int maxPreviewFrame = static_cast<int>(durationSeconds * fps);
            m_playheadFrame = (std::max)(0, (std::min)(m_playheadFrame, maxPreviewFrame));
            if (!m_previewState.GetDriver()->IsLoop() && m_playheadFrame >= maxPreviewFrame) {
                m_isPlaying = false;
            }
        }
        SyncPreviewTimelinePlayback();
    }

    if (ImGui::Button(ICON_FA_BACKWARD_STEP)) {
        m_playheadFrame = 0;
        m_isPlaying = false;
        if (m_previewState.IsActive()) {
            m_previewState.SetTime(0.0f);
        }
        SyncPreviewTimelinePlayback();
    }
    ImGui::SameLine();
    if (ImGui::Button(m_isPlaying ? ICON_FA_PAUSE : ICON_FA_PLAY)) {
        if (m_isPlaying) {
            m_isPlaying = false;
            SyncPreviewTimelinePlayback();
        } else if (m_previewState.IsActive()) {
            m_isPlaying = true;
            SyncPreviewTimelinePlayback();
        } else {
            StartSelectedAnimationPreview();
        }
    }
    ImGui::SameLine();
    if (ImGui::Button(ICON_FA_STOP)) {
        m_isPlaying = false;
        m_playheadFrame = 0;
        if (m_previewState.IsActive()) {
            m_previewState.SetTime(0.0f);
        }
        SyncPreviewTimelinePlayback();
    }
    ImGui::SameLine();
    if (ImGui::Button(ICON_FA_FORWARD_STEP)) {
        ++m_playheadFrame;
        if (m_previewState.IsActive()) {
            m_previewState.SetTime(m_playheadFrame / fps);
        }
        SyncPreviewTimelinePlayback();
    }
    ImGui::SameLine();

    int maxFrame = m_timelineAsset.GetFrameCount();
    if (maxFrame <= 0 && durationSeconds > 0.0f) {
        maxFrame = static_cast<int>(durationSeconds * fps);
    }
    if (maxFrame <= 0) maxFrame = 600;
    ImGui::SetNextItemWidth(100);
    if (ImGui::DragInt("##Frame", &m_playheadFrame, 1.0f, 0, maxFrame) && m_previewState.IsActive()) {
        m_previewState.SetTime(m_playheadFrame / fps);
        SyncPreviewTimelinePlayback();
    }
    ImGui::SameLine();
    ImGui::Text("/ %d", maxFrame);
    ImGui::SameLine();

    ImGui::SetNextItemWidth(50);
    if (ImGui::DragFloat("FPS", &m_timelineAsset.fps, 1.0f, 1.0f, 120.0f, "%.0f")) {
        m_timelineDirty = true;
    }
    ImGui::SameLine();

    ImGui::SetNextItemWidth(80);
    ImGui::SliderFloat("Zoom##TL", &m_timelineZoom, 0.2f, 5.0f, "%.1f");
    ImGui::SameLine();

    if (ImGui::Button(ICON_FA_FLOPPY_DISK)) {
        SaveTimelineDocument(false);
    }
    ImGui::SameLine();
    if (ImGui::Button(ICON_FA_FILE_EXPORT)) {
        SaveTimelineDocument(true);
    }

    ImGui::SameLine();
    ImGui::SetNextItemWidth(80);
    if (ImGui::DragFloat("Dur(s)", &m_timelineAsset.duration, 0.1f, 0.1f, 300.0f, "%.1f")) {
        m_timelineDirty = true;
    }
}
void PlayerEditorPanel::DrawTimelineTrackHeaders(float height)
{
    if (ImGui::Button(ICON_FA_PLUS " Track")) {
        ImGui::OpenPopup("AddTrackPopup");
    }

    if (ImGui::BeginPopup("AddTrackPopup")) {
        if (ImGui::MenuItem("Hitbox")) {
            TimelineTrack* track = m_timelineAsset.AddTrack(TimelineTrackType::Hitbox, GenerateDefaultTrackName(m_timelineAsset, TimelineTrackType::Hitbox));
            if (track) track->items.push_back(CreateDefaultTimelineItem(TimelineTrackType::Hitbox, m_playheadFrame));
            m_timelineDirty = true;
        }
        if (ImGui::MenuItem("VFX")) {
            TimelineTrack* track = m_timelineAsset.AddTrack(TimelineTrackType::VFX, GenerateDefaultTrackName(m_timelineAsset, TimelineTrackType::VFX));
            if (track) track->items.push_back(CreateDefaultTimelineItem(TimelineTrackType::VFX, m_playheadFrame));
            m_timelineDirty = true;
        }
        if (ImGui::MenuItem("Audio")) {
            TimelineTrack* track = m_timelineAsset.AddTrack(TimelineTrackType::Audio, GenerateDefaultTrackName(m_timelineAsset, TimelineTrackType::Audio));
            if (track) track->items.push_back(CreateDefaultTimelineItem(TimelineTrackType::Audio, m_playheadFrame));
            m_timelineDirty = true;
        }
        if (ImGui::MenuItem("CameraShake")) {
            TimelineTrack* track = m_timelineAsset.AddTrack(TimelineTrackType::CameraShake, GenerateDefaultTrackName(m_timelineAsset, TimelineTrackType::CameraShake));
            if (track) track->items.push_back(CreateDefaultTimelineItem(TimelineTrackType::CameraShake, m_playheadFrame));
            m_timelineDirty = true;
        }
        if (ImGui::MenuItem("Event")) {
            TimelineTrack* track = m_timelineAsset.AddTrack(TimelineTrackType::Event, GenerateDefaultTrackName(m_timelineAsset, TimelineTrackType::Event));
            if (track) track->items.push_back(CreateDefaultTimelineItem(TimelineTrackType::Event, m_playheadFrame));
            m_timelineDirty = true;
        }
        ImGui::EndPopup();
    }

    ImGui::Separator();

    // Track list
    for (int ti = 0; ti < (int)m_timelineAsset.tracks.size(); ++ti) {
        auto& track = m_timelineAsset.tracks[ti];
        ImGui::PushID(track.id);

        bool selected = (m_selectedTrackId == (int)track.id);

        // Mute toggle icon
        if (track.muted)
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.5f, 0.5f, 0.5f, 1.0f));

        if (ImGui::Selectable(track.name.c_str(), selected, 0, ImVec2(0, kTrackHeight))) {
            m_selectedTrackId = track.id;
            m_selectedItemIdx = -1;
            m_selectionCtx = SelectionContext::TimelineTrack;
        }

        if (track.muted)
            ImGui::PopStyleColor();

        // Context menu
        if (ImGui::BeginPopupContextItem()) {
            if (ImGui::Checkbox("Muted", &track.muted)) m_timelineDirty = true;
            if (ImGui::Checkbox("Locked", &track.locked)) m_timelineDirty = true;
            ImGui::Separator();
            if (ImGui::MenuItem(ICON_FA_TRASH " Delete Track")) {
                m_timelineAsset.RemoveTrack(track.id);
                m_timelineDirty = true;
                ImGui::EndPopup();
                ImGui::PopID();
                break;
            }
            ImGui::EndPopup();
        }

        ImGui::PopID();
    }
}

void PlayerEditorPanel::DrawTimelineGrid(float height)
{
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 canvasPos = ImGui::GetCursorScreenPos();
    ImVec2 canvasSize = ImGui::GetContentRegionAvail();

    float ppf = kDefaultPPF * m_timelineZoom;
    ppf = (std::max)(kMinPixelsPerFrame, ppf);

    int totalFrames = m_timelineAsset.GetFrameCount();
    if (totalFrames <= 0) totalFrames = 600;

    float totalWidth = totalFrames * ppf;
    float drawWidth = (std::max)(canvasSize.x, totalWidth);

    // Background
    dl->AddRectFilled(canvasPos, ImVec2(canvasPos.x + drawWidth, canvasPos.y + canvasSize.y),
        IM_COL32(28, 28, 28, 255));

    // Ruler
    float rulerH = 22.0f;
    int frameStep = (std::max)(1, (int)(40.0f / ppf));
    for (int f = 0; f <= totalFrames; f += frameStep) {
        float x = canvasPos.x + f * ppf;
        bool major = (f % (frameStep * 5) == 0);
        dl->AddLine(ImVec2(x, canvasPos.y), ImVec2(x, canvasPos.y + rulerH),
            major ? IM_COL32(100, 100, 100, 255) : IM_COL32(60, 60, 60, 255));
        if (major || ppf > 3.0f) {
            char buf[16]; snprintf(buf, sizeof(buf), "%d", f);
            dl->AddText(ImVec2(x + 2, canvasPos.y + 2), IM_COL32(170, 170, 170, 255), buf);
        }
    }

    // Track rows & items
    float yOff = rulerH;
    for (int ti = 0; ti < (int)m_timelineAsset.tracks.size(); ++ti) {
        auto& track = m_timelineAsset.tracks[ti];
        float trackY = canvasPos.y + yOff;

        ImU32 rowCol = (ti % 2 == 0) ? IM_COL32(32, 32, 32, 255) : IM_COL32(38, 38, 38, 255);
        dl->AddRectFilled(ImVec2(canvasPos.x, trackY),
            ImVec2(canvasPos.x + drawWidth, trackY + kTrackHeight), rowCol);

        ImU32 itemCol = track.muted ? IM_COL32(70, 70, 70, 180) : track.color;

        for (int ii = 0; ii < (int)track.items.size(); ++ii) {
            auto& item = track.items[ii];
            float x0 = canvasPos.x + item.startFrame * ppf;
            float x1 = canvasPos.x + item.endFrame * ppf;
            if (track.type == TimelineTrackType::Event) {
                x1 = x0 + (std::max)(12.0f, ppf * 0.75f);
            }
            float y0 = trackY + 2;
            float y1 = trackY + kTrackHeight - 2;

            bool isSelItem = (m_selectedTrackId == (int)track.id && m_selectedItemIdx == ii);
            bool leftResizeHovered = false;
            bool rightResizeHovered = false;
            bool resizingItem = false;

            // Item body with rounded corners
            dl->AddRectFilled(ImVec2(x0, y0), ImVec2(x1, y1), itemCol, 4.0f);
            if (isSelItem) {
                dl->AddRect(ImVec2(x0, y0), ImVec2(x1, y1), IM_COL32(255, 255, 255, 255), 4.0f, 0, 2.0f);
            }

            // Frame range label inside item
            if ((x1 - x0) > 40) {
                char lbl[32]; snprintf(lbl, sizeof(lbl), "%d-%d", item.startFrame, item.endFrame);
                dl->AddText(ImVec2(x0 + 3, y0 + 1), IM_COL32(255, 255, 255, 180), lbl);
            }

            if (!track.locked && track.type != TimelineTrackType::Event) {
                const float handleWidth = 10.0f;
                ImGui::SetCursorScreenPos(ImVec2(x0 - handleWidth * 0.5f, y0));
                ImGui::InvisibleButton(("item_l_" + std::to_string(track.id) + "_" + std::to_string(ii)).c_str(), ImVec2(handleWidth, y1 - y0));
                leftResizeHovered = ImGui::IsItemHovered();
                if (ImGui::IsItemActive() && ImGui::IsMouseDragging(0)) {
                    int frameDelta = static_cast<int>(ImGui::GetMouseDragDelta(0).x / ppf);
                    if (frameDelta != 0) {
                        item.startFrame = (std::max)(0, (std::min)(item.endFrame - 1, item.startFrame + frameDelta));
                        m_timelineDirty = true;
                        ImGui::ResetMouseDragDelta();
                    }
                    resizingItem = true;
                }

                ImGui::SetCursorScreenPos(ImVec2(x1 - handleWidth * 0.5f, y0));
                ImGui::InvisibleButton(("item_r_" + std::to_string(track.id) + "_" + std::to_string(ii)).c_str(), ImVec2(handleWidth, y1 - y0));
                rightResizeHovered = ImGui::IsItemHovered();
                if (ImGui::IsItemActive() && ImGui::IsMouseDragging(0)) {
                    int frameDelta = static_cast<int>(ImGui::GetMouseDragDelta(0).x / ppf);
                    if (frameDelta != 0) {
                        item.endFrame = (std::max)(item.startFrame + 1, item.endFrame + frameDelta);
                        m_timelineDirty = true;
                        ImGui::ResetMouseDragDelta();
                    }
                    resizingItem = true;
                }

                if (leftResizeHovered || rightResizeHovered) {
                    ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
                }
            }

            // Click / drag body after resize handles so the edges win.
            ImGui::SetCursorScreenPos(ImVec2(x0, y0));
            ImGui::InvisibleButton(("item_" + std::to_string(track.id) + "_" + std::to_string(ii)).c_str(),
                ImVec2((std::max)(x1 - x0, 4.0f), y1 - y0));

            if (ImGui::IsItemClicked(0)) {
                m_selectedTrackId = track.id;
                m_selectedItemIdx = ii;
                m_selectionCtx = SelectionContext::TimelineItem;
            }

            // Drag to move only when not resizing.
            if (!resizingItem && ImGui::IsItemActive() && ImGui::IsMouseDragging(0) && !track.locked) {
                float dx = ImGui::GetMouseDragDelta(0).x;
                int frameDelta = (int)(dx / ppf);
                if (frameDelta != 0) {
                    item.startFrame += frameDelta;
                    if (track.type == TimelineTrackType::Event) {
                        item.endFrame = item.startFrame;
                    } else {
                        item.endFrame += frameDelta;
                    }
                    if (item.startFrame < 0) { item.endFrame -= item.startFrame; item.startFrame = 0; }
                    m_timelineDirty = true;
                    ImGui::ResetMouseDragDelta();
                }
            }

            // Right-click: delete item
            if (ImGui::IsItemClicked(1)) {
                m_selectedTrackId = track.id;
                m_selectedItemIdx = ii;
                m_selectionCtx = SelectionContext::TimelineItem;
                ImGui::OpenPopup("ItemCtx");
            }
        }

          // Right-click on empty track area is intentionally disabled.
          ImGui::SetCursorScreenPos(ImVec2(canvasPos.x, trackY));
          ImGui::InvisibleButton(("trow_" + std::to_string(track.id)).c_str(),
              ImVec2(drawWidth, kTrackHeight));

        yOff += kTrackHeight;
    }

      // Item context menu
      if (ImGui::BeginPopup("ItemCtx")) {
          if (ImGui::MenuItem(ICON_FA_TRASH " Delete")) {
              for (auto& track : m_timelineAsset.tracks) {
                  if ((int)track.id == m_selectedTrackId && m_selectedItemIdx >= 0 &&
                    m_selectedItemIdx < (int)track.items.size())
                {
                    track.items.erase(track.items.begin() + m_selectedItemIdx);
                    m_selectedItemIdx = -1;
                    m_selectionCtx = SelectionContext::None;
                    m_timelineDirty = true;
                    break;
                }
            }
        }
        ImGui::EndPopup();
    }

    // Playhead
    float phX = canvasPos.x + m_playheadFrame * ppf;
    dl->AddLine(ImVec2(phX, canvasPos.y), ImVec2(phX, canvasPos.y + canvasSize.y),
        IM_COL32(255, 70, 70, 255), 2.0f);
    dl->AddTriangleFilled(
        ImVec2(phX - kPlayheadTriSize, canvasPos.y),
        ImVec2(phX + kPlayheadTriSize, canvasPos.y),
        ImVec2(phX, canvasPos.y + kPlayheadTriSize * 1.5f),
        IM_COL32(255, 70, 70, 255));

    // Click ruler to scrub
    ImGui::SetCursorScreenPos(canvasPos);
    ImGui::InvisibleButton("ruler_click", ImVec2(drawWidth, rulerH));
    if (ImGui::IsItemActive()) {
        float mx = ImGui::GetMousePos().x - canvasPos.x;
        m_playheadFrame = (std::max)(0, (std::min)(totalFrames, (int)(mx / ppf)));
        if (m_previewState.IsActive()) {
            const float fps = m_timelineAsset.fps > 0.0f ? m_timelineAsset.fps : 60.0f;
            m_previewState.SetTime(m_playheadFrame / fps);
        }
    }

    // Zoom with scroll wheel
    if (ImGui::IsWindowHovered()) {
        float wheel = ImGui::GetIO().MouseWheel;
        if (wheel != 0.0f) {
            m_timelineZoom *= (wheel > 0) ? 1.1f : 0.9f;
            m_timelineZoom = (std::max)(0.1f, (std::min)(8.0f, m_timelineZoom));
        }
    }

    // Dummy for scrollbar
    ImGui::SetCursorScreenPos(ImVec2(canvasPos.x + totalWidth, canvasPos.y + yOff + rulerH));
    ImGui::Dummy(ImVec2(0, 0));
}

// ############################################################################
//  PROPERTIES PANEL (context-sensitive)
// ############################################################################

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
    default:
        if (!m_stateMachineAsset.states.empty() || !m_stateMachineAsset.parameters.empty()) {
            DrawStateMachineParameterList();
        } else {
            ImGui::TextDisabled("Select a state, transition, track,");
            ImGui::TextDisabled("item, bone, or socket to view");
            ImGui::TextDisabled("its properties here.");
        }
        break;
    }

    ImGui::End();
}

bool PlayerEditorPanel::DrawAnimationSelector(const char* label, int* animIndex)
{
    if (!animIndex) return false;

    std::string preview = "None";
    const auto* selectedAnimation = m_model &&
        *animIndex >= 0 &&
        *animIndex < static_cast<int>(m_model->GetAnimations().size())
        ? &m_model->GetAnimations()[*animIndex]
        : nullptr;

    if (selectedAnimation) {
        preview = "[" + std::to_string(*animIndex) + "] " + selectedAnimation->name;
    } else if (*animIndex >= 0) {
        preview = "[" + std::to_string(*animIndex) + "] <invalid>";
    }

    bool changed = false;
    if (ImGui::BeginCombo(label, preview.c_str())) {
        const bool noneSelected = (*animIndex < 0);
        if (ImGui::Selectable("None", noneSelected)) {
            *animIndex = -1;
            changed = true;
        }
        if (noneSelected) {
            ImGui::SetItemDefaultFocus();
        }

        if (m_model) {
            const auto& animations = m_model->GetAnimations();
            for (int i = 0; i < static_cast<int>(animations.size()); ++i) {
                const bool selected = (*animIndex == i);
                std::string itemLabel = "[" + std::to_string(i) + "] " + animations[i].name;
                if (ImGui::Selectable(itemLabel.c_str(), selected)) {
                    *animIndex = i;
                    changed = true;
                }
                if (selected) {
                    ImGui::SetItemDefaultFocus();
                }
            }
        }

        ImGui::EndCombo();
    }

    if (m_model) {
        ImGui::TextDisabled("%d animation(s)", static_cast<int>(m_model->GetAnimations().size()));
    } else {
        ImGui::TextDisabled("No model assigned.");
    }

    return changed;
}

float PlayerEditorPanel::GetSelectedAnimationDurationSeconds() const
{
    if (m_timelineAsset.duration > 0.0f) {
        return m_timelineAsset.duration;
    }

    if (m_model &&
        m_selectedAnimIndex >= 0 &&
        m_selectedAnimIndex < static_cast<int>(m_model->GetAnimations().size())) {
        return (std::max)(0.0f, m_model->GetAnimations()[m_selectedAnimIndex].secondsLength);
    }

    return 10.0f;
}

void PlayerEditorPanel::AddStateTemplate(StateNodeType type, const DirectX::XMFLOAT2& graphPosition)
{
    StateNode* state = m_stateMachineAsset.AddState(GetStateTypeLabel(type), type);
    if (!state) {
        return;
    }

    state->position = graphPosition;
    state->loopAnimation = (type == StateNodeType::Locomotion);
    state->canInterrupt = (type == StateNodeType::Locomotion);
    state->animSpeed = 1.0f;

    if (m_stateMachineAsset.defaultStateId == 0) {
        m_stateMachineAsset.defaultStateId = state->id;
    }

    m_selectedNodeId = state->id;
    m_selectedTransitionId = 0;
    m_selectionCtx = SelectionContext::StateNode;
    m_stateMachineDirty = true;
}

void PlayerEditorPanel::StartSelectedAnimationPreview()
{
    const bool hasPreviewTarget = m_registry && !Entity::IsNull(m_previewEntity) && m_registry->IsAlive(m_previewEntity);
    if (!hasPreviewTarget) {
        return;
    }

    if (!m_model || m_model->GetAnimations().empty()) {
        return;
    }

    if (m_selectedAnimIndex < 0 || m_selectedAnimIndex >= static_cast<int>(m_model->GetAnimations().size())) {
        m_selectedAnimIndex = 0;
    }

    AnimatorService::Instance().EnsureAnimator(m_previewEntity);
    if (!m_previewState.IsActive()) {
        m_previewState.EnterPreview(m_previewEntity);
    }

    RebuildPreviewTimelineRuntimeData();
    m_playheadFrame = 0;
    m_previewState.SetAnimationIndex(m_selectedAnimIndex);
    m_previewState.SetTime(0.0f);
    m_isPlaying = true;
    SyncPreviewTimelinePlayback();
}

void PlayerEditorPanel::PreviewStateNode(uint32_t stateId, bool restartTimeline)
{
    StateNode* state = m_stateMachineAsset.FindState(stateId);
    if (!state) {
        return;
    }

    m_selectedNodeId = stateId;
    m_selectionCtx = SelectionContext::StateNode;

    if (state->animationIndex >= 0) {
        m_selectedAnimIndex = state->animationIndex;
    }
    if (!state->timelineAssetPath.empty() && (restartTimeline || m_timelineAssetPath != state->timelineAssetPath)) {
        OpenTimelineFromPath(state->timelineAssetPath);
    } else {
        RebuildPreviewTimelineRuntimeData();
    }

    StartSelectedAnimationPreview();
    if (m_previewState.IsActive()) {
        m_previewState.SetLoop(state->loopAnimation);
    }
    if (CanUsePreviewEntity()) {
        if (PlaybackComponent* playback = m_registry->GetComponent<PlaybackComponent>(m_previewEntity)) {
            playback->looping = state->loopAnimation;
        }
    }
}

void PlayerEditorPanel::DrawStateNodeInspector()
{
    auto* state = m_stateMachineAsset.FindState(m_selectedNodeId);
    if (!state) { ImGui::TextDisabled("No state selected"); return; }

    ImGui::Text(ICON_FA_CIRCLE_NODES " State: %s", state->name.c_str());
    if (m_stateMachineAsset.defaultStateId == state->id) {
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.2f, 1.0f), "[Default]");
    }
    ImGui::Separator();

    char nameBuf[128];
    strncpy_s(nameBuf, state->name.c_str(), _TRUNCATE);
    if (ImGui::InputText("Name", nameBuf, sizeof(nameBuf))) { state->name = nameBuf; m_stateMachineDirty = true; }

    int typeInt = static_cast<int>(state->type);
    const char* typeNames[] = { "Locomotion", "Action", "Dodge", "Jump", "Damage", "Dead", "Custom" };
    if (ImGui::Combo("Type", &typeInt, typeNames, IM_ARRAYSIZE(typeNames))) {
        state->type = static_cast<StateNodeType>(typeInt);
        m_stateMachineDirty = true;
    }

    ImGui::Separator();
    ImGui::Text("Animation");
    if (DrawAnimationSelector("Animation", &state->animationIndex)) m_stateMachineDirty = true;
    if (ImGui::Checkbox("Loop", &state->loopAnimation)) m_stateMachineDirty = true;
    if (ImGui::DragFloat("Speed", &state->animSpeed, 0.01f, 0.0f, 5.0f, "%.2f")) m_stateMachineDirty = true;
    if (ImGui::Checkbox("Can Interrupt", &state->canInterrupt)) m_stateMachineDirty = true;
    if (ImGui::Button(ICON_FA_PLAY " Preview State")) {
        PreviewStateNode(state->id, false);
    }
    ImGui::SameLine();
    if (ImGui::Button(ICON_FA_ROTATE_LEFT " Preview From Start")) {
        PreviewStateNode(state->id, true);
    }

    ImGui::Separator();
    ImGui::Text("Timeline Asset");
    char pathBuf[256];
    strncpy_s(pathBuf, state->timelineAssetPath.c_str(), _TRUNCATE);
    if (ImGui::InputText("Path", pathBuf, sizeof(pathBuf))) { state->timelineAssetPath = pathBuf; m_stateMachineDirty = true; }
    if (!state->timelineAssetPath.empty()) {
        if (ImGui::Button(ICON_FA_ARROW_RIGHT " Open in Timeline")) {
            OpenTimelineFromPath(state->timelineAssetPath);
        }
    }

    ImGui::Separator();
    ImGui::Text("Input Map");
    const auto& editingMap = m_inputMappingTab.GetEditingMap();
    ImGui::TextDisabled("Action Map: %s", m_inputMappingTab.GetActionMapPath().empty() ? "(none)" : std::filesystem::path(m_inputMappingTab.GetActionMapPath()).filename().string().c_str());
    ImGui::TextDisabled("Actions: %d / Axes: %d", static_cast<int>(editingMap.actions.size()), static_cast<int>(editingMap.axes.size()));
    ImGui::Separator();
    ImGui::Text("Outgoing Transitions");
    auto transitions = m_stateMachineAsset.GetTransitionsFrom(m_selectedNodeId);
    for (auto* t : transitions) {
        auto* to = m_stateMachineAsset.FindState(t->toState);
        std::string label = to ? (ICON_FA_ARROW_RIGHT " " + to->name) : "-> ???";
        if (ImGui::Selectable(label.c_str(), m_selectedTransitionId == t->id)) {
            m_selectedTransitionId = t->id;
            m_selectionCtx = SelectionContext::Transition;
        }
    }
}

void PlayerEditorPanel::DrawStateMachineParameterList()
{
    ImGui::Text(ICON_FA_SLIDERS " Parameters");
    ImGui::Separator();

    for (int i = 0; i < static_cast<int>(m_stateMachineAsset.parameters.size()); ++i) {
        auto& parameter = m_stateMachineAsset.parameters[i];
        ImGui::PushID(i);

        char nameBuf[64];
        strncpy_s(nameBuf, parameter.name.c_str(), _TRUNCATE);
        if (ImGui::InputText("Name", nameBuf, sizeof(nameBuf))) {
            parameter.name = nameBuf;
            m_stateMachineDirty = true;
        }

        int typeInt = static_cast<int>(parameter.type);
        const char* typeNames[] = { "Float", "Int", "Bool", "Trigger" };
        if (ImGui::Combo("Type", &typeInt, typeNames, IM_ARRAYSIZE(typeNames))) {
            parameter.type = static_cast<ParameterType>(typeInt);
            m_stateMachineDirty = true;
        }

        if (ImGui::DragFloat("Default", &parameter.defaultValue, 0.1f)) {
            m_stateMachineDirty = true;
        }

        if (ImGui::Button(ICON_FA_TRASH " Remove")) {
            m_stateMachineAsset.parameters.erase(m_stateMachineAsset.parameters.begin() + i);
            m_stateMachineDirty = true;
            ImGui::PopID();
            break;
        }

        ImGui::Separator();
        ImGui::PopID();
    }

    if (ImGui::Button(ICON_FA_PLUS " Add Parameter")) {
        ParameterDef parameter;
        parameter.name = "Param";
        m_stateMachineAsset.parameters.push_back(std::move(parameter));
        m_stateMachineDirty = true;
    }
}

void PlayerEditorPanel::DrawTransitionConditionEditor(StateTransition* trans)
{
    auto* from = m_stateMachineAsset.FindState(trans->fromState);
    auto* to   = m_stateMachineAsset.FindState(trans->toState);

    ImGui::Text(ICON_FA_ARROW_RIGHT " Transition");
    ImGui::Text("%s -> %s", from ? from->name.c_str() : "?", to ? to->name.c_str() : "?");
    ImGui::Separator();

    if (ImGui::DragInt("Priority", &trans->priority, 1, 0, 100)) m_stateMachineDirty = true;
    if (ImGui::Checkbox("Has Exit Time", &trans->hasExitTime)) m_stateMachineDirty = true;
    if (trans->hasExitTime) {
        if (ImGui::DragFloat("Exit Time (0-1)", &trans->exitTimeNormalized, 0.01f, 0.0f, 1.0f)) m_stateMachineDirty = true;
    }
    if (ImGui::DragFloat("Blend Duration", &trans->blendDuration, 0.01f, 0.0f, 2.0f)) m_stateMachineDirty = true;

    ImGui::Separator();
    ImGui::TextDisabled("Summary");
    for (int ci = 0; ci < (int)trans->conditions.size(); ++ci) {
        const auto& cond = trans->conditions[ci];
        std::string summary = std::string(ResolveConditionTypeLabel(cond.type)) + " ";
        if (cond.type == ConditionType::Input || cond.type == ConditionType::Parameter) {
            summary += (cond.param[0] != '\0') ? cond.param : "(unset)";
            summary += " ";
        }
        summary += ResolveCompareOpLabel(cond.compare);
        summary += " ";
        if (cond.type == ConditionType::AnimEnd) {
            summary += (cond.value != 0.0f) ? "true" : "false";
        } else {
            summary += std::to_string(cond.value);
        }
        ImGui::BulletText("%s", summary.c_str());
    }
    if (trans->conditions.empty()) {
        ImGui::TextDisabled("No conditions.");
    }
    ImGui::Separator();
    ImGui::Text("Conditions (%d):", (int)trans->conditions.size());

    for (int ci = 0; ci < (int)trans->conditions.size(); ++ci) {
        auto& cond = trans->conditions[ci];
        ImGui::PushID(ci);

        int typeInt = static_cast<int>(cond.type);
        const char* condTypes[] = { "Input", "Timer", "AnimEnd", "Health", "Stamina", "Parameter" };
        ImGui::SetNextItemWidth(90);
        if (ImGui::Combo("##T", &typeInt, condTypes, IM_ARRAYSIZE(condTypes))) {
            cond.type = static_cast<ConditionType>(typeInt);
            m_stateMachineDirty = true;
        }

        ImGui::SameLine();
        ImGui::SetNextItemWidth(140);
        if (cond.type == ConditionType::Input && !m_inputMappingTab.GetEditingMap().actions.empty()) {
            const auto& actions = m_inputMappingTab.GetEditingMap().actions;
            const char* preview = cond.param[0] != '\0' ? cond.param : "(select action)";
            if (ImGui::BeginCombo("##P", preview)) {
                for (const auto& action : actions) {
                    const bool selected = strcmp(cond.param, action.actionName.c_str()) == 0;
                    if (ImGui::Selectable(action.actionName.c_str(), selected)) {
                        strncpy_s(cond.param, action.actionName.c_str(), _TRUNCATE);
                        m_stateMachineDirty = true;
                    }
                    if (selected) {
                        ImGui::SetItemDefaultFocus();
                    }
                }
                ImGui::EndCombo();
            }
        } else if (ImGui::InputText("##P", cond.param, sizeof(cond.param))) {
            m_stateMachineDirty = true;
        }

        ImGui::SameLine();
        int cmpInt = static_cast<int>(cond.compare);
        const char* cmpOps[] = { "==", "!=", ">", "<", ">=", "<=" };
        ImGui::SetNextItemWidth(45);
        if (ImGui::Combo("##C", &cmpInt, cmpOps, IM_ARRAYSIZE(cmpOps))) {
            cond.compare = static_cast<CompareOp>(cmpInt);
            m_stateMachineDirty = true;
        }

        ImGui::SameLine();
        ImGui::SetNextItemWidth(55);
        if (ImGui::DragFloat("##V", &cond.value, 0.1f)) m_stateMachineDirty = true;

        ImGui::SameLine();
        if (ImGui::Button(ICON_FA_XMARK)) {
            trans->conditions.erase(trans->conditions.begin() + ci);
            m_stateMachineDirty = true;
            ImGui::PopID();
            break;
        }

        ImGui::PopID();
    }

    if (ImGui::Button(ICON_FA_PLUS " Condition")) {
        trans->conditions.push_back(TransitionCondition{});
        m_stateMachineDirty = true;
    }

    ImGui::Separator();
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.6f, 0.15f, 0.15f, 1.0f));
    if (ImGui::Button(ICON_FA_TRASH " Delete Transition")) {
        m_stateMachineAsset.RemoveTransition(m_selectedTransitionId);
        m_selectedTransitionId = 0;
        m_selectionCtx = SelectionContext::None;
        m_stateMachineDirty = true;
    }
    ImGui::PopStyleColor();
}

void PlayerEditorPanel::DrawTimelineItemInspector()
{
    //ImGui::Text(ICON_FA_CLOCK " Timeline Item");
    //ImGui::Separator();

    for (auto& track : m_timelineAsset.tracks) {
        if ((int)track.id != m_selectedTrackId) continue;
        if (m_selectedItemIdx < 0 || m_selectedItemIdx >= (int)track.items.size()) break;

        auto& item = track.items[m_selectedItemIdx];

        ImGui::Text("Track: %s", track.name.c_str());
        ImGui::TextDisabled("Resize the bar directly on the timeline.");

        ImGui::Separator();

        // Payload editing based on track type
        switch (track.type) {
        case TimelineTrackType::Hitbox:
            ImGui::Text(ICON_FA_CROSSHAIRS " Hitbox Payload");
            if (ImGui::DragInt("Node Index", &item.hitbox.nodeIndex, 1, 0, 200)) m_timelineDirty = true;
            if (m_selectedBoneIndex >= 0 && !m_selectedBoneName.empty()) {
                if (ImGui::Button("Use Selected Bone")) {
                    item.hitbox.nodeIndex = m_selectedBoneIndex;
                    m_timelineDirty = true;
                }
                ImGui::SameLine();
                ImGui::TextDisabled("%s", m_selectedBoneName.c_str());
            }
            if (m_selectedSocketIdx >= 0 && m_selectedSocketIdx < static_cast<int>(m_sockets.size())) {
                const NodeSocket& socket = m_sockets[m_selectedSocketIdx];
                if (ImGui::Button("Use Selected Socket")) {
                    item.hitbox.nodeIndex = socket.cachedBoneIndex;
                    item.hitbox.offsetLocal = socket.offsetPos;
                    m_timelineDirty = true;
                }
                ImGui::SameLine();
                ImGui::TextDisabled("%s", socket.name.c_str());
            }
            if (ImGui::DragFloat3("Offset", &item.hitbox.offsetLocal.x, 0.01f)) m_timelineDirty = true;
            if (ImGui::DragFloat("Radius", &item.hitbox.radius, 0.01f, 0.0f, 10.0f)) m_timelineDirty = true;
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
                }
            }
            break;

        case TimelineTrackType::VFX:
            ImGui::Text(ICON_FA_WAND_MAGIC_SPARKLES " VFX Payload");
            if (ImGui::InputText("Asset ID", item.vfx.assetId, sizeof(item.vfx.assetId))) m_timelineDirty = true;
            if (ImGui::DragInt("Node Index", &item.vfx.nodeIndex, 1, 0, 200)) m_timelineDirty = true;
            if (m_selectedBoneIndex >= 0 && !m_selectedBoneName.empty()) {
                if (ImGui::Button("Use Selected Bone##VFX")) {
                    item.vfx.nodeIndex = m_selectedBoneIndex;
                    m_timelineDirty = true;
                }
                ImGui::SameLine();
                ImGui::TextDisabled("%s", m_selectedBoneName.c_str());
            }
            if (m_selectedSocketIdx >= 0 && m_selectedSocketIdx < static_cast<int>(m_sockets.size())) {
                const NodeSocket& socket = m_sockets[m_selectedSocketIdx];
                if (ImGui::Button("Use Selected Socket##VFX")) {
                    item.vfx.nodeIndex = socket.cachedBoneIndex;
                    item.vfx.offsetLocal = socket.offsetPos;
                    item.vfx.offsetRotDeg = socket.offsetRotDeg;
                    item.vfx.offsetScale = socket.offsetScale;
                    m_timelineDirty = true;
                }
                ImGui::SameLine();
                ImGui::TextDisabled("%s", socket.name.c_str());
            }
            if (ImGui::DragFloat3("Offset", &item.vfx.offsetLocal.x, 0.01f)) m_timelineDirty = true;
            if (ImGui::DragFloat3("Rotation", &item.vfx.offsetRotDeg.x, 0.1f)) m_timelineDirty = true;
            if (ImGui::DragFloat3("Scale", &item.vfx.offsetScale.x, 0.01f, 0.01f, 10.0f)) m_timelineDirty = true;
            break;

        case TimelineTrackType::Audio:
            ImGui::Text(ICON_FA_VOLUME_HIGH " Audio Payload");
            if (ImGui::InputText("Asset ID", item.audio.assetId, sizeof(item.audio.assetId))) m_timelineDirty = true;
            if (ImGui::DragFloat("Volume", &item.audio.volume, 0.01f, 0.0f, 1.0f)) m_timelineDirty = true;
            if (ImGui::DragFloat("Pitch", &item.audio.pitch, 0.01f, 0.1f, 3.0f)) m_timelineDirty = true;
            if (ImGui::Checkbox("3D Audio", &item.audio.is3D)) m_timelineDirty = true;
            if (ImGui::DragInt("Node Index", &item.audio.nodeIndex, 1, 0, 200)) m_timelineDirty = true;
            if (m_selectedBoneIndex >= 0 && !m_selectedBoneName.empty()) {
                if (ImGui::Button("Use Selected Bone##Audio")) {
                    item.audio.nodeIndex = m_selectedBoneIndex;
                    m_timelineDirty = true;
                }
                ImGui::SameLine();
                ImGui::TextDisabled("%s", m_selectedBoneName.c_str());
            }
            if (ImGui::Checkbox("Loop", &item.audio.loop)) m_timelineDirty = true;
            break;

        case TimelineTrackType::CameraShake:
            ImGui::Text(ICON_FA_CAMERA " Camera Shake Payload");
            if (ImGui::DragFloat("Duration", &item.shake.duration, 0.01f, 0.0f, 5.0f)) m_timelineDirty = true;
            if (ImGui::DragFloat("Amplitude", &item.shake.amplitude, 0.01f, 0.0f, 10.0f)) m_timelineDirty = true;
            if (ImGui::DragFloat("Frequency", &item.shake.frequency, 0.1f, 0.0f, 100.0f)) m_timelineDirty = true;
            if (ImGui::DragFloat("Decay", &item.shake.decay, 0.01f, 0.0f, 10.0f)) m_timelineDirty = true;
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
}

// ############################################################################
//  ANIMATOR PANEL
// ############################################################################

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
                    m_selectedAnimIndex = i;
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

// ############################################################################
//  INPUT PANEL
// ############################################################################

void PlayerEditorPanel::DrawInputPanel()
{
    if (!ImGui::Begin(kPEInputTitle)) { ImGui::End(); return; }
    m_inputMappingTab.Draw(m_registry);
    ImGui::End();
}

