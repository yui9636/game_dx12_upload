#include "PlayerEditorPanel.h"
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <imgui.h>
#include <imgui_internal.h>
#include <algorithm>
#include <cmath>
#include "Icon/IconsFontAwesome7.h"
#include "TimelineAssetSerializer.h"
#include "StateMachineAssetSerializer.h"
#include "ImGuiRenderer.h"
#include "Model/Model.h"

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

// ============================================================================
// Main Draw — Host DockSpace
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

        DrawViewportPanel();
        DrawSkeletonPanel();
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

    // ── Create internal DockSpace ──
    ImGuiID dockId = ImGui::GetID("PlayerEditorDock");
    ImGui::DockSpace(dockId, ImVec2(0, 0), ImGuiDockNodeFlags_None);

    if (m_needsLayoutRebuild) {
        BuildDockLayout(dockId);
        m_needsLayoutRebuild = false;
    }

    ImGui::End(); // Host window

    // ── Draw each sub-window (they dock into the host's DockSpace) ──
    DrawViewportPanel();
    DrawSkeletonPanel();
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
//  ┌────────────┬──────────────────────┬──────────────────┐
//  │ StateMachine│  3D Viewport        │ Properties       │
//  │ (NodeGraph) │  (center, large)    │ (Details)        │
//  │             │                     ├──────────────────┤
//  │             │                     │ Animator / Input │
//  ├─────────────┴─────────────────────┴──────────────────┤
//  │ Timeline  (full width, bottom)                       │
//  └──────────────────────────────────────────────────────┘
//

void PlayerEditorPanel::BuildDockLayout(unsigned int dockspaceId)
{
    ImGuiID dockId = dockspaceId;
    ImGui::DockBuilderRemoveNode(dockId);
    ImGui::DockBuilderAddNode(dockId, ImGuiDockNodeFlags_DockSpace);
    ImGui::DockBuilderSetNodeSize(dockId, ImVec2(1500, 850));

    ImGuiID mainId = dockId;

    // 1. Bottom: Timeline (full width, 35% height — like UE's Notify/Track panel)
    ImGuiID bottomId;
    ImGui::DockBuilderSplitNode(mainId, ImGuiDir_Down, 0.35f, &bottomId, &mainId);

    // 2. Left: StateMachine (25% width of top area)
    ImGuiID leftId;
    ImGui::DockBuilderSplitNode(mainId, ImGuiDir_Left, 0.25f, &leftId, &mainId);

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
    if (!ImGui::Begin(kPEViewportTitle)) { ImGui::End(); return; }

    ImVec2 avail = ImGui::GetContentRegionAvail();

    if (m_viewportTexture) {
        // Display the dedicated render target
        void* texId = ImGuiRenderer::GetTextureID(m_viewportTexture);
        if (texId) {
            ImGui::Image((ImTextureID)texId, avail);
        }
    } else {
        // Placeholder: dark background with instructions
        ImDrawList* dl = ImGui::GetWindowDrawList();
        ImVec2 pos = ImGui::GetCursorScreenPos();
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
    if (ImGui::IsItemHovered()) {
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

// ############################################################################
//  MODEL SETTER
// ############################################################################

void PlayerEditorPanel::SetModel(const Model* model)
{
    if (m_model == model) return;
    m_model = model;
    m_selectedBoneIndex = -1;
    m_selectedBoneName.clear();
}

// ############################################################################
//  SKELETON PANEL — Bone Tree + Socket Management
// ############################################################################

void PlayerEditorPanel::DrawSkeletonPanel()
{
    if (!ImGui::Begin(kPESkeletonTitle)) { ImGui::End(); return; }

    if (!m_model) {
        ImGui::TextDisabled("No model assigned.");
        ImGui::TextDisabled("Select an entity with a model");
        ImGui::TextDisabled("to display its bone hierarchy.");
        ImGui::End();
        return;
    }

    const auto& nodes = m_model->GetNodes();

    // ── Selected bone info bar ──
    if (m_selectedBoneIndex >= 0 && m_selectedBoneIndex < (int)nodes.size()) {
        ImGui::TextColored(ImVec4(0.4f, 0.9f, 0.4f, 1.0f), ICON_FA_BONE " %s", m_selectedBoneName.c_str());
        ImGui::SameLine();
        ImGui::TextDisabled("[%d]", m_selectedBoneIndex);

        // "Apply to Item" button — sets the nodeIndex on selected timeline item
        ImGui::SameLine();
        if (ImGui::Button(ICON_FA_ARROW_RIGHT " Apply to Item")) {
            for (auto& track : m_timelineAsset.tracks) {
                if ((int)track.id != m_selectedTrackId) continue;
                if (m_selectedItemIdx < 0 || m_selectedItemIdx >= (int)track.items.size()) break;
                auto& item = track.items[m_selectedItemIdx];
                switch (track.type) {
                case TimelineTrackType::Hitbox:     item.hitbox.nodeIndex = m_selectedBoneIndex; break;
                case TimelineTrackType::VFX:        item.vfx.nodeIndex   = m_selectedBoneIndex; break;
                case TimelineTrackType::Audio:      item.audio.nodeIndex = m_selectedBoneIndex; break;
                default: break;
                }
            }
        }
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Set this bone as nodeIndex on selected timeline item");
    } else {
        ImGui::TextDisabled("No bone selected");
    }
    ImGui::Separator();

    // ── Search filter ──
    ImGui::SetNextItemWidth(-1);
    ImGui::InputTextWithHint("##BoneSearch", ICON_FA_MAGNIFYING_GLASS " Search bones...",
        m_boneSearchFilter, sizeof(m_boneSearchFilter));

    ImGui::Separator();

    // ── Bone tree / flat list ──
    ImGui::BeginChild("BoneTree", ImVec2(0, -ImGui::GetFrameHeightWithSpacing() * 5 - 8), ImGuiChildFlags_Borders);

    if (m_boneSearchFilter[0] == '\0') {
        // Hierarchical tree view: start from root nodes (parentIndex == -1 or 0)
        for (int i = 0; i < (int)nodes.size(); ++i) {
            if (nodes[i].parentIndex < 0) {
                DrawBoneTreeNode(i);
            }
        }
        // Also draw orphan root (parentIndex == 0 and index != 0)
        // Usually node 0 is the scene root, handled by parentIndex == -1
    } else {
        // Flat filtered list
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

    // ── Socket section ──
    ImGui::Separator();
    DrawSocketList();

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
            }
        }
        if (ImGui::MenuItem(ICON_FA_WAND_MAGIC_SPARKLES " Set as VFX Node")) {
            for (auto& track : m_timelineAsset.tracks) {
                if ((int)track.id != m_selectedTrackId) continue;
                if (m_selectedItemIdx < 0 || m_selectedItemIdx >= (int)track.items.size()) break;
                if (track.type == TimelineTrackType::VFX)
                    track.items[m_selectedItemIdx].vfx.nodeIndex = m_selectedBoneIndex;
            }
        }
        if (ImGui::MenuItem(ICON_FA_PLUG " Create Socket Here")) {
            NodeSocket sock;
            sock.name = "Socket_" + node.name;
            sock.parentBoneName = node.name;
            sock.cachedBoneIndex = nodeIndex;
            m_sockets.push_back(sock);
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

void PlayerEditorPanel::DrawSocketList()
{
    ImGui::Text(ICON_FA_PLUG " Sockets (%d)", (int)m_sockets.size());

    if (ImGui::Button(ICON_FA_PLUS " Add Socket")) {
        NodeSocket sock;
        sock.name = "NewSocket";
        if (m_selectedBoneIndex >= 0 && !m_selectedBoneName.empty()) {
            sock.parentBoneName = m_selectedBoneName;
            sock.cachedBoneIndex = m_selectedBoneIndex;
            sock.name = "Socket_" + m_selectedBoneName;
        }
        m_sockets.push_back(sock);
    }

    for (int si = 0; si < (int)m_sockets.size(); ++si) {
        auto& sock = m_sockets[si];
        ImGui::PushID(si);

        bool selected = (m_selectedSocketIdx == si);
        std::string label = ICON_FA_PLUG " " + sock.name + " -> " + sock.parentBoneName;
        if (ImGui::Selectable(label.c_str(), selected)) {
            m_selectedSocketIdx = si;
            m_selectionCtx = SelectionContext::Socket;
        }

        ImGui::PopID();
    }
}

// ############################################################################
//  STATE MACHINE PANEL
// ############################################################################

void PlayerEditorPanel::DrawStateMachinePanel()
{
    if (!ImGui::Begin(kPEStateMachineTitle)) { ImGui::End(); return; }

    // Toolbar
    if (ImGui::Button(ICON_FA_FLOPPY_DISK " Save")) {
        if (!m_stateMachineAsset.name.empty()) {
            std::string path = "Assets/StateMachine/" + m_stateMachineAsset.name + ".statemachine.json";
            StateMachineAssetSerializer::Save(path, m_stateMachineAsset);
        }
    }
    ImGui::SameLine();
    if (ImGui::Button(ICON_FA_ARROWS_TO_CIRCLE " Fit")) {
        // Reset graph offset to center
        m_graphOffset = { 200, 150 };
        m_graphZoom = 1.0f;
    }
    ImGui::SameLine();
    ImGui::SetNextItemWidth(80);
    ImGui::SliderFloat("Zoom##SM", &m_graphZoom, 0.3f, 3.0f, "%.1f");
    ImGui::SameLine();

    // Connection mode indicator
    if (m_isConnecting) {
        ImGui::TextColored(ImVec4(1, 0.8f, 0.2f, 1), ICON_FA_LINK " Connecting... (ESC cancel)");
        if (ImGui::IsKeyPressed(ImGuiKey_Escape)) {
            m_isConnecting = false;
            m_connectFromNodeId = 0;
        }
    }

    ImGui::Separator();

    // Canvas
    ImVec2 canvasSize = ImGui::GetContentRegionAvail();
    DrawNodeGraph(canvasSize);

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
                TimelineAssetSerializer::Load(state.timelineAssetPath, m_timelineAsset);
            }
        }

        // Drag node
        if (ImGui::IsItemActive() && ImGui::IsMouseDragging(0) && !m_isConnecting) {
            ImVec2 delta = ImGui::GetMouseDragDelta(0);
            state.position.x += delta.x / m_graphZoom;
            state.position.y += delta.y / m_graphZoom;
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

        const char* types[] = { "Locomotion", "Action", "Dodge", "Jump", "Damage", "Dead", "Custom" };
        StateNodeType typeEnums[] = {
            StateNodeType::Locomotion, StateNodeType::Action, StateNodeType::Dodge,
            StateNodeType::Jump, StateNodeType::Damage, StateNodeType::Dead, StateNodeType::Custom
        };

        ImGui::TextDisabled("Add State:");
        for (int i = 0; i < 7; ++i) {
            if (ImGui::MenuItem(types[i])) {
                auto* s = m_stateMachineAsset.AddState(types[i], typeEnums[i]);
                s->position = { posX, posY };
                if (i == 0) s->loopAnimation = true;
            }
        }
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
    // Play controls
    if (ImGui::Button(ICON_FA_BACKWARD_STEP)) { m_playheadFrame = 0; m_isPlaying = false; }
    ImGui::SameLine();
    if (ImGui::Button(m_isPlaying ? ICON_FA_PAUSE : ICON_FA_PLAY)) { m_isPlaying = !m_isPlaying; }
    ImGui::SameLine();
    if (ImGui::Button(ICON_FA_STOP)) { m_isPlaying = false; m_playheadFrame = 0; }
    ImGui::SameLine();
    if (ImGui::Button(ICON_FA_FORWARD_STEP)) { m_playheadFrame++; }
    ImGui::SameLine();

    // Frame display
    int maxFrame = m_timelineAsset.GetFrameCount();
    if (maxFrame <= 0) maxFrame = 600;
    ImGui::SetNextItemWidth(100);
    ImGui::DragInt("##Frame", &m_playheadFrame, 1.0f, 0, maxFrame);
    ImGui::SameLine();
    ImGui::Text("/ %d", maxFrame);
    ImGui::SameLine();

    // FPS
    ImGui::SetNextItemWidth(50);
    ImGui::DragFloat("FPS", &m_timelineAsset.fps, 1.0f, 1.0f, 120.0f, "%.0f");
    ImGui::SameLine();

    // Zoom
    ImGui::SetNextItemWidth(80);
    ImGui::SliderFloat("Zoom##TL", &m_timelineZoom, 0.2f, 5.0f, "%.1f");
    ImGui::SameLine();

    // Save
    if (ImGui::Button(ICON_FA_FLOPPY_DISK)) {
        if (!m_timelineAsset.name.empty()) {
            std::string path = "Assets/Timeline/" + m_timelineAsset.name + ".timeline.json";
            TimelineAssetSerializer::Save(path, m_timelineAsset);
        }
    }

    // Duration
    ImGui::SameLine();
    ImGui::SetNextItemWidth(80);
    ImGui::DragFloat("Dur(s)", &m_timelineAsset.duration, 0.1f, 0.1f, 300.0f, "%.1f");

    ImGui::Separator();
}

void PlayerEditorPanel::DrawTimelineTrackHeaders(float height)
{
    // Add track button
    if (ImGui::Button(ICON_FA_PLUS " Track")) {
        ImGui::OpenPopup("AddTrackPopup");
    }

    if (ImGui::BeginPopup("AddTrackPopup")) {
        for (int i = 0; i < 8; ++i) {
            if (ImGui::MenuItem(kTrackTypeNames[i])) {
                m_timelineAsset.AddTrack(static_cast<TimelineTrackType>(i), kTrackTypeNames[i]);
            }
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
            ImGui::Checkbox("Muted", &track.muted);
            ImGui::Checkbox("Locked", &track.locked);
            ImGui::Separator();
            if (ImGui::MenuItem("Add Item Here")) {
                TimelineItem item;
                item.startFrame = m_playheadFrame;
                item.endFrame = m_playheadFrame + 15;
                track.items.push_back(item);
            }
            ImGui::Separator();
            if (ImGui::MenuItem(ICON_FA_TRASH " Delete Track")) {
                m_timelineAsset.RemoveTrack(track.id);
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
            float y0 = trackY + 2;
            float y1 = trackY + kTrackHeight - 2;

            bool isSelItem = (m_selectedTrackId == (int)track.id && m_selectedItemIdx == ii);

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

            // Click / drag
            ImGui::SetCursorScreenPos(ImVec2(x0, y0));
            ImGui::InvisibleButton(("item_" + std::to_string(track.id) + "_" + std::to_string(ii)).c_str(),
                ImVec2((std::max)(x1 - x0, 4.0f), y1 - y0));

            if (ImGui::IsItemClicked(0)) {
                m_selectedTrackId = track.id;
                m_selectedItemIdx = ii;
                m_selectionCtx = SelectionContext::TimelineItem;
            }

            // Drag to move
            if (ImGui::IsItemActive() && ImGui::IsMouseDragging(0) && !track.locked) {
                float dx = ImGui::GetMouseDragDelta(0).x;
                int frameDelta = (int)(dx / ppf);
                if (frameDelta != 0) {
                    item.startFrame += frameDelta;
                    item.endFrame += frameDelta;
                    if (item.startFrame < 0) { item.endFrame -= item.startFrame; item.startFrame = 0; }
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

        // Right-click on empty track area
        ImGui::SetCursorScreenPos(ImVec2(canvasPos.x, trackY));
        ImGui::InvisibleButton(("trow_" + std::to_string(track.id)).c_str(),
            ImVec2(drawWidth, kTrackHeight));
        if (ImGui::IsItemClicked(1) && !track.locked) {
            float mx = ImGui::GetMousePos().x - canvasPos.x;
            int clickFrame = (int)(mx / ppf);
            TimelineItem item;
            item.startFrame = clickFrame;
            item.endFrame = clickFrame + 15;
            track.items.push_back(item);
            m_selectedTrackId = track.id;
            m_selectedItemIdx = (int)track.items.size() - 1;
            m_selectionCtx = SelectionContext::TimelineItem;
        }

        yOff += kTrackHeight;
    }

    // Item context menu
    if (ImGui::BeginPopup("ItemCtx")) {
        if (ImGui::MenuItem(ICON_FA_COPY " Duplicate")) {
            for (auto& track : m_timelineAsset.tracks) {
                if ((int)track.id == m_selectedTrackId && m_selectedItemIdx >= 0 &&
                    m_selectedItemIdx < (int)track.items.size())
                {
                    auto copy = track.items[m_selectedItemIdx];
                    copy.startFrame += 10;
                    copy.endFrame += 10;
                    track.items.push_back(copy);
                    break;
                }
            }
        }
        if (ImGui::MenuItem(ICON_FA_TRASH " Delete")) {
            for (auto& track : m_timelineAsset.tracks) {
                if ((int)track.id == m_selectedTrackId && m_selectedItemIdx >= 0 &&
                    m_selectedItemIdx < (int)track.items.size())
                {
                    track.items.erase(track.items.begin() + m_selectedItemIdx);
                    m_selectedItemIdx = -1;
                    m_selectionCtx = SelectionContext::None;
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
            if (ImGui::InputText("Name", nameBuf, sizeof(nameBuf))) track.name = nameBuf;

            int typeInt = static_cast<int>(track.type);
            if (ImGui::Combo("Type", &typeInt, kTrackTypeNames, 8))
                track.type = static_cast<TimelineTrackType>(typeInt);

            ImGui::Checkbox("Muted", &track.muted);
            ImGui::Checkbox("Locked", &track.locked);

            ImU32 col = track.color;
            float rgba[4] = {
                ((col >> 0) & 0xFF) / 255.0f, ((col >> 8) & 0xFF) / 255.0f,
                ((col >> 16) & 0xFF) / 255.0f, ((col >> 24) & 0xFF) / 255.0f };
            if (ImGui::ColorEdit4("Color", rgba, ImGuiColorEditFlags_NoInputs)) {
                track.color = IM_COL32(
                    (int)(rgba[0] * 255), (int)(rgba[1] * 255),
                    (int)(rgba[2] * 255), (int)(rgba[3] * 255));
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
            if (ImGui::InputText("Name", nameBuf, sizeof(nameBuf))) sock.name = nameBuf;

            ImGui::Text("Parent Bone: %s [%d]", sock.parentBoneName.c_str(), sock.cachedBoneIndex);
            if (m_selectedBoneIndex >= 0 && !m_selectedBoneName.empty()) {
                ImGui::SameLine();
                if (ImGui::SmallButton("Use Selected")) {
                    sock.parentBoneName = m_selectedBoneName;
                    sock.cachedBoneIndex = m_selectedBoneIndex;
                }
            }

            ImGui::DragFloat3("Offset Pos", &sock.offsetPos.x, 0.01f);
            ImGui::DragFloat3("Offset Rot", &sock.offsetRotDeg.x, 0.1f);
            ImGui::DragFloat3("Offset Scale", &sock.offsetScale.x, 0.01f, 0.01f, 10.0f);

            ImGui::Separator();
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.6f, 0.15f, 0.15f, 1.0f));
            if (ImGui::Button(ICON_FA_TRASH " Delete Socket")) {
                m_sockets.erase(m_sockets.begin() + m_selectedSocketIdx);
                m_selectedSocketIdx = -1;
                m_selectionCtx = SelectionContext::None;
            }
            ImGui::PopStyleColor();
        }
        break;
    }
    default:
        ImGui::TextDisabled("Select a state, transition, track,");
        ImGui::TextDisabled("item, bone, or socket to view");
        ImGui::TextDisabled("its properties here.");
        break;
    }

    ImGui::End();
}

void PlayerEditorPanel::DrawStateNodeInspector()
{
    auto* state = m_stateMachineAsset.FindState(m_selectedNodeId);
    if (!state) { ImGui::TextDisabled("No state selected"); return; }

    ImGui::Text(ICON_FA_CIRCLE_NODES " State: %s", state->name.c_str());
    ImGui::Separator();

    char nameBuf[128];
    strncpy_s(nameBuf, state->name.c_str(), _TRUNCATE);
    if (ImGui::InputText("Name", nameBuf, sizeof(nameBuf))) state->name = nameBuf;

    int typeInt = static_cast<int>(state->type);
    const char* typeNames[] = { "Locomotion", "Action", "Dodge", "Jump", "Damage", "Dead", "Custom" };
    if (ImGui::Combo("Type", &typeInt, typeNames, IM_ARRAYSIZE(typeNames)))
        state->type = static_cast<StateNodeType>(typeInt);

    ImGui::Separator();
    ImGui::Text("Animation");
    ImGui::DragInt("Anim Index", &state->animationIndex, 1, -1, 200);
    ImGui::Checkbox("Loop", &state->loopAnimation);
    ImGui::DragFloat("Speed", &state->animSpeed, 0.01f, 0.0f, 5.0f, "%.2f");
    ImGui::Checkbox("Can Interrupt", &state->canInterrupt);

    ImGui::Separator();
    ImGui::Text("Timeline Asset");
    char pathBuf[256];
    strncpy_s(pathBuf, state->timelineAssetPath.c_str(), _TRUNCATE);
    if (ImGui::InputText("Path", pathBuf, sizeof(pathBuf))) state->timelineAssetPath = pathBuf;
    if (!state->timelineAssetPath.empty()) {
        if (ImGui::Button(ICON_FA_ARROW_RIGHT " Open in Timeline")) {
            TimelineAssetSerializer::Load(state->timelineAssetPath, m_timelineAsset);
        }
    }

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

void PlayerEditorPanel::DrawTransitionConditionEditor(StateTransition* trans)
{
    auto* from = m_stateMachineAsset.FindState(trans->fromState);
    auto* to   = m_stateMachineAsset.FindState(trans->toState);

    ImGui::Text(ICON_FA_ARROW_RIGHT " Transition");
    ImGui::Text("%s -> %s", from ? from->name.c_str() : "?", to ? to->name.c_str() : "?");
    ImGui::Separator();

    ImGui::DragInt("Priority", &trans->priority, 1, 0, 100);
    ImGui::Checkbox("Has Exit Time", &trans->hasExitTime);
    if (trans->hasExitTime) {
        ImGui::DragFloat("Exit Time (0-1)", &trans->exitTimeNormalized, 0.01f, 0.0f, 1.0f);
    }
    ImGui::DragFloat("Blend Duration", &trans->blendDuration, 0.01f, 0.0f, 2.0f);

    ImGui::Separator();
    ImGui::Text("Conditions (%d):", (int)trans->conditions.size());

    for (int ci = 0; ci < (int)trans->conditions.size(); ++ci) {
        auto& cond = trans->conditions[ci];
        ImGui::PushID(ci);

        int typeInt = static_cast<int>(cond.type);
        const char* condTypes[] = { "Input", "Timer", "AnimEnd", "Health", "Stamina", "Parameter" };
        ImGui::SetNextItemWidth(90);
        if (ImGui::Combo("##T", &typeInt, condTypes, IM_ARRAYSIZE(condTypes)))
            cond.type = static_cast<ConditionType>(typeInt);

        ImGui::SameLine();
        ImGui::SetNextItemWidth(70);
        ImGui::InputText("##P", cond.param, sizeof(cond.param));

        ImGui::SameLine();
        int cmpInt = static_cast<int>(cond.compare);
        const char* cmpOps[] = { "==", "!=", ">", "<", ">=", "<=" };
        ImGui::SetNextItemWidth(45);
        if (ImGui::Combo("##C", &cmpInt, cmpOps, IM_ARRAYSIZE(cmpOps)))
            cond.compare = static_cast<CompareOp>(cmpInt);

        ImGui::SameLine();
        ImGui::SetNextItemWidth(55);
        ImGui::DragFloat("##V", &cond.value, 0.1f);

        ImGui::SameLine();
        if (ImGui::Button(ICON_FA_XMARK)) {
            trans->conditions.erase(trans->conditions.begin() + ci);
            ImGui::PopID();
            break;
        }

        ImGui::PopID();
    }

    if (ImGui::Button(ICON_FA_PLUS " Condition")) {
        trans->conditions.push_back(TransitionCondition{});
    }

    ImGui::Separator();
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.6f, 0.15f, 0.15f, 1.0f));
    if (ImGui::Button(ICON_FA_TRASH " Delete Transition")) {
        m_stateMachineAsset.RemoveTransition(m_selectedTransitionId);
        m_selectedTransitionId = 0;
        m_selectionCtx = SelectionContext::None;
    }
    ImGui::PopStyleColor();
}

void PlayerEditorPanel::DrawTimelineItemInspector()
{
    ImGui::Text(ICON_FA_CLOCK " Timeline Item");
    ImGui::Separator();

    for (auto& track : m_timelineAsset.tracks) {
        if ((int)track.id != m_selectedTrackId) continue;
        if (m_selectedItemIdx < 0 || m_selectedItemIdx >= (int)track.items.size()) break;

        auto& item = track.items[m_selectedItemIdx];

        ImGui::Text("Track: %s", track.name.c_str());
        ImGui::DragInt("Start Frame", &item.startFrame, 1.0f, 0, 99999);
        ImGui::DragInt("End Frame", &item.endFrame, 1.0f, 0, 99999);

        int dur = item.endFrame - item.startFrame;
        ImGui::Text("Duration: %d frames (%.3fs)", dur,
            m_timelineAsset.fps > 0 ? dur / m_timelineAsset.fps : 0.0f);

        ImGui::Separator();

        // Payload editing based on track type
        switch (track.type) {
        case TimelineTrackType::Hitbox:
            ImGui::Text(ICON_FA_CROSSHAIRS " Hitbox Payload");
            ImGui::DragInt("Node Index", &item.hitbox.nodeIndex, 1, 0, 200);
            ImGui::DragFloat3("Offset", &item.hitbox.offsetLocal.x, 0.01f);
            ImGui::DragFloat("Radius", &item.hitbox.radius, 0.01f, 0.0f, 10.0f);
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
                }
            }
            break;

        case TimelineTrackType::VFX:
            ImGui::Text(ICON_FA_WAND_MAGIC_SPARKLES " VFX Payload");
            ImGui::InputText("Asset ID", item.vfx.assetId, sizeof(item.vfx.assetId));
            ImGui::DragInt("Node Index", &item.vfx.nodeIndex, 1, 0, 200);
            ImGui::DragFloat3("Offset", &item.vfx.offsetLocal.x, 0.01f);
            ImGui::DragFloat3("Rotation", &item.vfx.offsetRotDeg.x, 0.1f);
            ImGui::DragFloat3("Scale", &item.vfx.offsetScale.x, 0.01f, 0.01f, 10.0f);
            break;

        case TimelineTrackType::Audio:
            ImGui::Text(ICON_FA_VOLUME_HIGH " Audio Payload");
            ImGui::InputText("Asset ID", item.audio.assetId, sizeof(item.audio.assetId));
            ImGui::DragFloat("Volume", &item.audio.volume, 0.01f, 0.0f, 1.0f);
            ImGui::DragFloat("Pitch", &item.audio.pitch, 0.01f, 0.1f, 3.0f);
            ImGui::Checkbox("3D Audio", &item.audio.is3D);
            ImGui::DragInt("Node Index", &item.audio.nodeIndex, 1, 0, 200);
            ImGui::Checkbox("Loop", &item.audio.loop);
            break;

        case TimelineTrackType::CameraShake:
            ImGui::Text(ICON_FA_CAMERA " Camera Shake Payload");
            ImGui::DragFloat("Duration", &item.shake.duration, 0.01f, 0.0f, 5.0f);
            ImGui::DragFloat("Amplitude", &item.shake.amplitude, 0.01f, 0.0f, 10.0f);
            ImGui::DragFloat("Frequency", &item.shake.frequency, 0.1f, 0.0f, 100.0f);
            ImGui::DragFloat("Decay", &item.shake.decay, 0.01f, 0.0f, 10.0f);
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

    bool previewing = m_previewState.IsActive();

    if (previewing) {
        ImGui::TextColored(ImVec4(0.2f, 1.0f, 0.2f, 1.0f), ICON_FA_CIRCLE " Preview Active");
        ImGui::Separator();

        ImGui::DragInt("Animation Index", &m_selectedAnimIndex, 1, -1, 200);
        m_previewState.SetAnimationIndex(m_selectedAnimIndex);

        float t = m_previewState.GetDriver()->GetTime();
        float maxT = m_timelineAsset.duration > 0 ? m_timelineAsset.duration : 10.0f;
        if (ImGui::SliderFloat("Time", &t, 0.0f, maxT, "%.3f")) {
            m_previewState.SetTime(t);
        }

        bool loop = m_previewState.GetDriver()->IsLoop();
        if (ImGui::Checkbox("Loop", &loop)) m_previewState.SetLoop(loop);

        ImGui::Separator();
        if (ImGui::Button(ICON_FA_STOP " Stop Preview")) {
            m_previewState.ExitPreview();
        }
    } else {
        ImGui::TextDisabled("Select an entity with AnimatorComponent");
        ImGui::TextDisabled("and enter preview mode to begin.");
        ImGui::Separator();
        ImGui::DragInt("Animation Index", &m_selectedAnimIndex, 1, -1, 200);
    }

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
