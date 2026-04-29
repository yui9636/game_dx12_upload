#include "GameLoopEditorPanel.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <algorithm>
#include <cctype>
#include <cfloat>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <vector>

#include <imgui.h>
#include <imgui_internal.h>

#include "Console/Logger.h"
#include "Engine/EngineKernel.h"
#include "GameLoopRuntime.h"
#include "System/PathResolver.h"

namespace
{
    constexpr float kNodeWidth = 260.0f;
    constexpr float kNodeHeight = 92.0f;
    constexpr float kPinRadius = 7.0f;
    constexpr float kFitPadding = 80.0f;

    constexpr const char* kGraphTitle = "GameLoop Graph##GLE";
    constexpr const char* kOutlinerTitle = "GameLoop Outliner##GLE";
    constexpr const char* kPropertiesTitle = "GameLoop Properties##GLE";
    constexpr const char* kValidateTitle = "GameLoop Validate##GLE";
    constexpr const char* kRuntimeTitle = "GameLoop Runtime##GLE";

    GameLoopAsset& Asset() { return EngineKernel::Instance().GetGameLoopAsset(); }
    const GameLoopRuntime& Runtime() { return EngineKernel::Instance().GetGameLoopRuntime(); }

    const char* kActionLabels[] = { "Confirm", "Cancel", "Retry" };
    constexpr int kActionCount = sizeof(kActionLabels) / sizeof(kActionLabels[0]);

    const char* kActorLabels[] = { "None", "Player", "Enemy", "NPC", "Neutral" };
    const ActorType kActorValues[] = {
        ActorType::None, ActorType::Player, ActorType::Enemy, ActorType::NPC, ActorType::Neutral
    };
    constexpr int kActorCount = sizeof(kActorValues) / sizeof(kActorValues[0]);

    const char* kConditionLabels[] = {
        "None", "InputPressed", "UIButtonClicked", "TimerElapsed",
        "ActorDead", "AllActorsDead", "ActorMovedDistance", "RuntimeFlag",
        "StateMachineState", "TimelineEvent", "CustomEvent"
    };
    const GameLoopConditionType kConditionValues[] = {
        GameLoopConditionType::None,
        GameLoopConditionType::InputPressed,
        GameLoopConditionType::UIButtonClicked,
        GameLoopConditionType::TimerElapsed,
        GameLoopConditionType::ActorDead,
        GameLoopConditionType::AllActorsDead,
        GameLoopConditionType::ActorMovedDistance,
        GameLoopConditionType::RuntimeFlag,
        GameLoopConditionType::StateMachineState,
        GameLoopConditionType::TimelineEvent,
        GameLoopConditionType::CustomEvent,
    };
    constexpr int kConditionCount = sizeof(kConditionValues) / sizeof(kConditionValues[0]);

    const char* kLoadingLabels[] = { "Immediate", "FadeOnly", "LoadingOverlay" };
    const GameLoopLoadingMode kLoadingValues[] = {
        GameLoopLoadingMode::Immediate,
        GameLoopLoadingMode::FadeOnly,
        GameLoopLoadingMode::LoadingOverlay,
    };
    constexpr int kLoadingCount = sizeof(kLoadingValues) / sizeof(kLoadingValues[0]);

    ImVec2 AddVec2(const ImVec2& a, const ImVec2& b) { return ImVec2(a.x + b.x, a.y + b.y); }
    ImVec2 SubVec2(const ImVec2& a, const ImVec2& b) { return ImVec2(a.x - b.x, a.y - b.y); }
    float ClampF(float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }
    float MinF(float a, float b) { return a < b ? a : b; }
    float MaxF(float a, float b) { return a > b ? a : b; }

    ImVec2 LerpVec2(const ImVec2& a, const ImVec2& b, float t)
    {
        return ImVec2(a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t);
    }

    ImVec2 CubicPoint(const ImVec2& p0, const ImVec2& p1, const ImVec2& p2, const ImVec2& p3, float t)
    {
        const ImVec2 a = LerpVec2(p0, p1, t);
        const ImVec2 b = LerpVec2(p1, p2, t);
        const ImVec2 c = LerpVec2(p2, p3, t);
        const ImVec2 d = LerpVec2(a, b, t);
        const ImVec2 e = LerpVec2(b, c, t);
        return LerpVec2(d, e, t);
    }

    float DistanceSqToSegment(const ImVec2& p, const ImVec2& a, const ImVec2& b)
    {
        const float vx = b.x - a.x;
        const float vy = b.y - a.y;
        const float wx = p.x - a.x;
        const float wy = p.y - a.y;
        const float lenSq = vx * vx + vy * vy;
        float t = lenSq > 0.0001f ? (wx * vx + wy * vy) / lenSq : 0.0f;
        t = ClampF(t, 0.0f, 1.0f);
        const float dx = p.x - (a.x + vx * t);
        const float dy = p.y - (a.y + vy * t);
        return dx * dx + dy * dy;
    }

    bool IsPointNearBezier(const ImVec2& p, const ImVec2& p0, const ImVec2& p1, const ImVec2& p2, const ImVec2& p3, float radius)
    {
        const float radiusSq = radius * radius;
        ImVec2 prev = p0;
        for (int i = 1; i <= 22; ++i) {
            const ImVec2 cur = CubicPoint(p0, p1, p2, p3, static_cast<float>(i) / 22.0f);
            if (DistanceSqToSegment(p, prev, cur) <= radiusSq) {
                return true;
            }
            prev = cur;
        }
        return false;
    }

    const char* ActionLabel(int actionIndex)
    {
        return (actionIndex >= 0 && actionIndex < kActionCount) ? kActionLabels[actionIndex] : "Unknown";
    }

    const char* ActorLabel(ActorType actorType)
    {
        switch (actorType) {
        case ActorType::Player:  return "Player";
        case ActorType::Enemy:   return "Enemy";
        case ActorType::NPC:     return "NPC";
        case ActorType::Neutral: return "Neutral";
        default:                 return "None";
        }
    }

    const char* LoadingLabel(GameLoopLoadingMode mode)
    {
        switch (mode) {
        case GameLoopLoadingMode::FadeOnly:       return "Fade";
        case GameLoopLoadingMode::LoadingOverlay: return "Overlay";
        default:                                  return "Immediate";
        }
    }

    int FindConditionIndex(GameLoopConditionType type)
    {
        for (int i = 0; i < kConditionCount; ++i) {
            if (kConditionValues[i] == type) return i;
        }
        return 0;
    }

    int FindActorIndex(ActorType actorType)
    {
        for (int i = 0; i < kActorCount; ++i) {
            if (kActorValues[i] == actorType) return i;
        }
        return 0;
    }

    int FindLoadingIndex(GameLoopLoadingMode mode)
    {
        for (int i = 0; i < kLoadingCount; ++i) {
            if (kLoadingValues[i] == mode) return i;
        }
        return 0;
    }

    bool LooksAbsolutePath(const std::string& path)
    {
        if (path.size() >= 3 && std::isalpha(static_cast<unsigned char>(path[0])) && path[1] == ':' &&
            (path[2] == '/' || path[2] == '\\')) {
            return true;
        }
        return path.rfind("\\\\", 0) == 0 || path.rfind("//", 0) == 0;
    }

    bool ContainsParentTraversal(const std::string& path)
    {
        std::string normalized = path;
        std::replace(normalized.begin(), normalized.end(), '\\', '/');
        return normalized == ".." ||
            normalized.rfind("../", 0) == 0 ||
            normalized.find("/../") != std::string::npos ||
            (normalized.size() >= 3 && normalized.compare(normalized.size() - 3, 3, "/..") == 0);
    }

    int NodePathSeverity(const GameLoopNode& node)
    {
        if (node.scenePath.empty() || LooksAbsolutePath(node.scenePath) || ContainsParentTraversal(node.scenePath)) {
            return 2;
        }
        const std::string normalized = NormalizeGameLoopScenePath(node.scenePath);
        if (normalized.empty()) {
            return 2;
        }
        std::error_code ec;
        if (!std::filesystem::exists(PathResolver::Resolve(normalized), ec)) {
            return 1;
        }
        return 0;
    }

    ImU32 SeverityColor(int severity)
    {
        if (severity >= 2) return IM_COL32(235, 75, 85, 255);
        if (severity == 1) return IM_COL32(236, 184, 72, 255);
        return IM_COL32(72, 184, 112, 255);
    }

    const char* SeverityText(int severity)
    {
        if (severity >= 2) return "E";
        if (severity == 1) return "W";
        return "OK";
    }
}

GameLoopEditorPanel::GameLoopEditorPanel()
{
    std::strncpy(m_loadPathBuf, m_currentPath.string().c_str(), sizeof(m_loadPathBuf) - 1);
    std::strncpy(m_saveAsPathBuf, m_currentPath.string().c_str(), sizeof(m_saveAsPathBuf) - 1);
}

void GameLoopEditorPanel::Draw(bool* p_open, bool* outFocused)
{
    ImGui::SetNextWindowSize(ImVec2(1280.0f, 760.0f), ImGuiCond_FirstUseEver);
    const ImGuiWindowFlags flags =
        ImGuiWindowFlags_MenuBar |
        ImGuiWindowFlags_NoScrollbar |
        ImGuiWindowFlags_NoScrollWithMouse;

    if (!ImGui::Begin("GameLoop Editor", p_open, flags)) {
        if (outFocused) *outFocused = false;
        ImGui::End();
        return;
    }

    if (outFocused) *outFocused = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);

    DrawToolbar();

    ImGuiID dockId = ImGui::GetID("GameLoopEditorDockSpace");
    ImGui::DockSpace(dockId, ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_None);
    if (m_needsLayoutRebuild) {
        BuildDockLayout(dockId);
        m_needsLayoutRebuild = false;
    }

    ImGui::End();

    DrawGraphPanel();
    DrawOutlinerPanel();
    DrawPropertiesPanel();
    DrawValidatePanel();
    DrawRuntimePanel();
    DrawFloatingTransitionEditor();
}

void GameLoopEditorPanel::DrawToolbar()
{
    if (!ImGui::BeginMenuBar()) {
        return;
    }

    if (ImGui::BeginMenu("File")) {
        if (ImGui::MenuItem("New Default Loop")) DoNewDefault();
        if (ImGui::MenuItem("Load...")) {
            std::strncpy(m_loadPathBuf, m_currentPath.string().c_str(), sizeof(m_loadPathBuf) - 1);
            ImGui::OpenPopup("Load GameLoop");
        }
        if (ImGui::MenuItem("Save", "Ctrl+S")) DoSave();
        if (ImGui::MenuItem("Save As...")) {
            std::strncpy(m_saveAsPathBuf, m_currentPath.string().c_str(), sizeof(m_saveAsPathBuf) - 1);
            ImGui::OpenPopup("Save GameLoop As");
        }
        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Create")) {
        if (ImGui::MenuItem("Title Node")) AddNodeAt(NodeTemplate::Title, { 0.0f, 0.0f });
        if (ImGui::MenuItem("Battle Node")) AddNodeAt(NodeTemplate::Battle, { 400.0f, 0.0f });
        if (ImGui::MenuItem("Result Node")) AddNodeAt(NodeTemplate::Result, { 800.0f, 0.0f });
        if (ImGui::MenuItem("Custom Scene Node")) AddNodeAt(NodeTemplate::Custom, { 0.0f, 160.0f });
        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("View")) {
        if (ImGui::MenuItem("Fit Graph", "A")) RequestFitGraph();
        if (ImGui::MenuItem("Zoom 100%")) m_graphZoom = 1.0f;
        if (ImGui::MenuItem("Rebuild Layout")) {
            m_needsLayoutRebuild = true;
            RequestFitGraph();
        }
        ImGui::EndMenu();
    }

    if (ImGui::MenuItem("Validate")) {
        DoValidate();
    }

    ImGui::SameLine();
    ImGui::TextDisabled("%s%s", m_dirty ? "*" : "", m_currentPath.filename().string().c_str());

    ImGui::EndMenuBar();

    if (ImGui::BeginPopupModal("Load GameLoop", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::InputText("Path", m_loadPathBuf, sizeof(m_loadPathBuf));
        if (ImGui::Button("Load")) {
            DoLoad(std::filesystem::path(m_loadPathBuf));
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel")) ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }

    if (ImGui::BeginPopupModal("Save GameLoop As", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::InputText("Path", m_saveAsPathBuf, sizeof(m_saveAsPathBuf));
        if (ImGui::Button("Save As")) {
            DoSaveAs(std::filesystem::path(m_saveAsPathBuf));
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel")) ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }
}

void GameLoopEditorPanel::BuildDockLayout(unsigned int dockspaceId)
{
    ImGuiID dockId = dockspaceId;
    ImGui::DockBuilderRemoveNode(dockId);
    ImGui::DockBuilderAddNode(dockId, ImGuiDockNodeFlags_DockSpace);
    ImGui::DockBuilderSetNodeSize(dockId, ImGui::GetMainViewport()->Size);

    ImGuiID topId = dockId;
    ImGuiID bottomId = 0;
    ImGui::DockBuilderSplitNode(topId, ImGuiDir_Down, 0.24f, &bottomId, &topId);

    ImGuiID outlinerId = 0;
    ImGui::DockBuilderSplitNode(topId, ImGuiDir_Left, 0.20f, &outlinerId, &topId);

    ImGuiID propertiesId = 0;
    ImGui::DockBuilderSplitNode(topId, ImGuiDir_Right, 0.23f, &propertiesId, &topId);

    ImGui::DockBuilderDockWindow(kGraphTitle, topId);
    ImGui::DockBuilderDockWindow(kOutlinerTitle, outlinerId);
    ImGui::DockBuilderDockWindow(kPropertiesTitle, propertiesId);
    ImGui::DockBuilderDockWindow(kValidateTitle, bottomId);
    ImGui::DockBuilderDockWindow(kRuntimeTitle, bottomId);
    ImGui::DockBuilderFinish(dockId);
}

void GameLoopEditorPanel::DrawGraphPanel()
{
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    const bool open = ImGui::Begin(kGraphTitle);
    ImGui::PopStyleVar();
    if (!open) {
        ImGui::End();
        return;
    }

    DrawGraphCanvas(ImGui::GetContentRegionAvail());
    ImGui::End();
}

void GameLoopEditorPanel::DrawOutlinerPanel()
{
    if (!ImGui::Begin(kOutlinerTitle)) {
        ImGui::End();
        return;
    }

    GameLoopAsset& asset = Asset();
    ImGui::SetNextItemWidth(-FLT_MIN);
    ImGui::InputTextWithHint("##GameLoopSearch", "Search nodes / transitions", m_outlinerSearch, sizeof(m_outlinerSearch));
    const std::string filter = m_outlinerSearch;

    if (ImGui::CollapsingHeader("Nodes", ImGuiTreeNodeFlags_DefaultOpen)) {
        for (const GameLoopNode& node : asset.nodes) {
            if (!filter.empty() && node.name.find(filter) == std::string::npos && node.scenePath.find(filter) == std::string::npos) {
                continue;
            }
            const int severity = NodePathSeverity(node);
            char label[320];
            std::snprintf(label, sizeof(label), "%s %s%s##node%u",
                SeverityText(severity),
                node.id == asset.startNodeId ? "* " : "",
                node.name.c_str(),
                node.id);
            if (ImGui::Selectable(label, m_selectedNodeId == node.id && m_selection == SelectionContext::Node)) {
                SelectNode(node.id);
            }
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("%s", node.scenePath.c_str());
            }
        }
    }

    if (ImGui::CollapsingHeader("Transitions", ImGuiTreeNodeFlags_DefaultOpen)) {
        for (const GameLoopTransition& transition : asset.transitions) {
            const GameLoopNode* from = asset.FindNode(transition.fromNodeId);
            const GameLoopNode* to = asset.FindNode(transition.toNodeId);
            std::string summary = BuildTransitionLabel(transition, true);
            if (!filter.empty() && summary.find(filter) == std::string::npos) {
                continue;
            }
            char label[384];
            std::snprintf(label, sizeof(label), "#%d %s -> %s : %s##transition%u",
                GetSourceLocalPriority(transition),
                from ? from->name.c_str() : "?",
                to ? to->name.c_str() : "?",
                summary.c_str(),
                transition.id);
            if (ImGui::Selectable(label, m_selectedTransitionId == transition.id && m_selection == SelectionContext::Transition)) {
                SelectTransition(transition.id);
            }
        }
    }

    ImGui::End();
}

void GameLoopEditorPanel::DrawPropertiesPanel()
{
    if (!ImGui::Begin(kPropertiesTitle)) {
        ImGui::End();
        return;
    }

    if (m_selection == SelectionContext::Node) {
        if (GameLoopNode* node = FindSelectedNode()) {
            DrawNodeProperties(*node);
        } else {
            ClearSelection();
        }
    } else if (m_selection == SelectionContext::Transition || m_selection == SelectionContext::Condition) {
        if (GameLoopTransition* transition = FindSelectedTransition()) {
            DrawTransitionProperties(*transition);
        } else {
            ClearSelection();
        }
    } else {
        const GameLoopAsset& asset = Asset();
        ImGui::TextDisabled("Select a node or transition.");
        ImGui::Separator();
        ImGui::Text("Nodes: %d", static_cast<int>(asset.nodes.size()));
        ImGui::Text("Transitions: %d", static_cast<int>(asset.transitions.size()));
        ImGui::Text("Start: %u", asset.startNodeId);
    }

    ImGui::End();
}

void GameLoopEditorPanel::DrawValidatePanel()
{
    if (!ImGui::Begin(kValidateTitle)) {
        ImGui::End();
        return;
    }

    if (!m_validatedOnce) {
        ImGui::TextDisabled("Validate has not been run.");
        ImGui::End();
        return;
    }

    ImGui::Text("Errors: %d   Warnings: %d", m_lastValidate.ErrorCount(), m_lastValidate.WarningCount());
    ImGui::Separator();
    for (const GameLoopValidateMessage& message : m_lastValidate.messages) {
        ImVec4 color(0.82f, 0.82f, 0.82f, 1.0f);
        const char* prefix = "[I]";
        if (message.severity == GameLoopValidateSeverity::Warning) {
            color = ImVec4(1.0f, 0.78f, 0.25f, 1.0f);
            prefix = "[W]";
        } else if (message.severity == GameLoopValidateSeverity::Error) {
            color = ImVec4(1.0f, 0.34f, 0.34f, 1.0f);
            prefix = "[E]";
        }
        ImGui::TextColored(color, "%s %s", prefix, message.message.c_str());
    }

    ImGui::End();
}

void GameLoopEditorPanel::DrawRuntimePanel()
{
    if (!ImGui::Begin(kRuntimeTitle)) {
        ImGui::End();
        return;
    }

    const GameLoopRuntime& runtime = Runtime();
    const GameLoopAsset& asset = Asset();
    const GameLoopNode* current = asset.FindNode(runtime.currentNodeId);
    const GameLoopNode* previous = asset.FindNode(runtime.previousNodeId);
    const GameLoopNode* pending = asset.FindNode(runtime.pendingNodeId);
    ImGui::Text("active=%s", runtime.isActive ? "true" : "false");
    ImGui::Text("previous=%s", previous ? previous->name.c_str() : "(none)");
    ImGui::Text("current=%s", current ? current->name.c_str() : "(none)");
    ImGui::Text("pending=%s%s", pending ? pending->name.c_str() : "(none)", runtime.sceneTransitionRequested ? " [requested]" : "");
    ImGui::Text("scene=%s", runtime.currentScenePath.empty() ? "(none)" : runtime.currentScenePath.c_str());
    ImGui::Text("pendingPath=%s", runtime.pendingScenePath.empty() ? "(none)" : runtime.pendingScenePath.c_str());
    ImGui::Text("waiting=%s forceReload=%s", runtime.waitingSceneLoad ? "true" : "false", runtime.forceReload ? "true" : "false");
    ImGui::Text("timer=%.2f", runtime.nodeTimer);

    ImGui::End();
}

void GameLoopEditorPanel::DrawGraphCanvas(const ImVec2& canvasSizeInput)
{
    GameLoopAsset& asset = Asset();
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 origin = ImGui::GetCursorScreenPos();
    ImVec2 canvasSize = canvasSizeInput;
    canvasSize.x = std::max(canvasSize.x, 120.0f);
    canvasSize.y = std::max(canvasSize.y, 120.0f);
    ImVec2 canvasEnd(origin.x + canvasSize.x, origin.y + canvasSize.y);

    if (m_graphFitRequested) {
        FitGraphToContent(canvasSize);
        m_graphFitRequested = false;
    }

    m_hoveredNodeId = 0;
    m_hoveredTransitionId = 0;
    m_hoveredInputNodeId = 0;
    m_hoveredOutputNodeId = 0;

    dl->AddRectFilled(origin, canvasEnd, IM_COL32(21, 22, 27, 255));
    DrawGrid(origin, canvasSize);

    const bool canvasHovered = ImGui::IsMouseHoveringRect(origin, canvasEnd, true);
    const bool graphFocused = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);

    if (graphFocused) {
        if (ImGui::IsKeyPressed(ImGuiKey_Delete)) DeleteSelected();
        if (ImGui::IsKeyPressed(ImGuiKey_Escape)) {
            m_isConnecting = false;
            m_connectFromNodeId = 0;
            m_connectionDragged = false;
            m_floatingTransitionEditorOpen = false;
            m_inlineNameNodeId = 0;
            m_inlinePathNodeId = 0;
        }
        if (ImGui::GetIO().KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_S)) DoSave();
        if (ImGui::IsKeyPressed(ImGuiKey_A)) RequestFitGraph();
        if (ImGui::IsKeyPressed(ImGuiKey_F)) FrameSelected(canvasSize);
    }

    if (canvasHovered && ImGui::GetIO().MouseWheel != 0.0f) {
        const ImVec2 mouse = ImGui::GetIO().MousePos;
        const DirectX::XMFLOAT2 before = ScreenToGraph(mouse, origin);
        const float wheel = ImGui::GetIO().MouseWheel;
        m_graphZoom *= (wheel > 0.0f) ? 1.10f : 0.90f;
        m_graphZoom = ClampF(m_graphZoom, 0.25f, 3.0f);
        m_graphOffset.x = mouse.x - origin.x - before.x * m_graphZoom;
        m_graphOffset.y = mouse.y - origin.y - before.y * m_graphZoom;
    }

    if (canvasHovered && ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
        m_rightPanActive = true;
        const ImVec2 mouse = ImGui::GetMousePos();
        m_rightPanStart = { mouse.x, mouse.y };
    }
    if (m_rightPanActive && ImGui::IsMouseDown(ImGuiMouseButton_Right)) {
        const ImVec2 delta = ImGui::GetIO().MouseDelta;
        if (delta.x != 0.0f || delta.y != 0.0f) {
            m_graphOffset.x += delta.x;
            m_graphOffset.y += delta.y;
            ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeAll);
        }
    }
    if (canvasHovered && (ImGui::IsMouseDragging(ImGuiMouseButton_Middle, 0.0f) ||
        (ImGui::IsKeyDown(ImGuiKey_Space) && ImGui::IsMouseDragging(ImGuiMouseButton_Left, 0.0f)))) {
        const ImVec2 delta = ImGui::GetIO().MouseDelta;
        m_graphOffset.x += delta.x;
        m_graphOffset.y += delta.y;
        ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeAll);
    }

    dl->PushClipRect(origin, canvasEnd, true);
    for (GameLoopTransition& transition : asset.transitions) {
        DrawTransitionEdge(transition, origin);
    }
    for (GameLoopNode& node : asset.nodes) {
        DrawNode(node, origin);
    }

    if (m_isConnecting) {
        if (GameLoopNode* from = asset.FindNode(m_connectFromNodeId)) {
            const ImVec2 fromPos = GraphToScreen(from->graphPos, origin);
            const ImVec2 start(fromPos.x + kNodeWidth * m_graphZoom, fromPos.y + kNodeHeight * 0.5f * m_graphZoom);
            const ImVec2 mouse = ImGui::GetIO().MousePos;
            const ImVec2 c1(start.x + 120.0f * m_graphZoom, start.y);
            const ImVec2 c2(mouse.x - 120.0f * m_graphZoom, mouse.y);
            dl->AddBezierCubic(start, c1, c2, mouse, IM_COL32(255, 210, 90, 235), 2.5f);
            if (m_hoveredNodeId != 0 || m_hoveredInputNodeId != 0) {
                dl->AddCircle(mouse, 12.0f, IM_COL32(95, 235, 145, 255), 0, 2.0f);
            }
        }
        if (ImGui::IsMouseDragging(ImGuiMouseButton_Left, 4.0f)) {
            m_connectionDragged = true;
        }
    }
    dl->PopClipRect();

    const bool overObject = m_hoveredNodeId != 0 || m_hoveredInputNodeId != 0 || m_hoveredOutputNodeId != 0;
    if (canvasHovered && !overObject && m_hoveredTransitionId != 0) {
        if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) SelectTransition(m_hoveredTransitionId);
        if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) OpenFloatingTransitionEditor(m_hoveredTransitionId, ImGui::GetIO().MousePos);
        if (ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
            SelectTransition(m_hoveredTransitionId);
            ImGui::OpenPopup("GLETransitionContext");
        }
    }

    if (m_isConnecting && ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
        const uint32_t targetNode = m_hoveredInputNodeId != 0 ? m_hoveredInputNodeId : m_hoveredNodeId;
        if (targetNode != 0) {
            AddTransition(m_connectFromNodeId, targetNode, true);
            m_isConnecting = false;
            m_connectFromNodeId = 0;
            m_connectionDragged = false;
        } else if (m_connectionDragged && canvasHovered) {
            m_quickCreateFromNodeId = m_connectFromNodeId;
            m_quickCreateGraphPos = ScreenToGraph(ImGui::GetIO().MousePos, origin);
            ImGui::OpenPopup("GLEQuickCreateNode");
            m_isConnecting = false;
            m_connectFromNodeId = 0;
            m_connectionDragged = false;
        } else {
            m_isConnecting = false;
            m_connectFromNodeId = 0;
            m_connectionDragged = false;
        }
    }

    if (ImGui::IsMouseReleased(ImGuiMouseButton_Right) && m_rightPanActive) {
        const ImVec2 mouse = ImGui::GetMousePos();
        const float dx = mouse.x - m_rightPanStart.x;
        const float dy = mouse.y - m_rightPanStart.y;
        const bool wasClick = std::fabs(dx) < 4.0f && std::fabs(dy) < 4.0f;
        m_rightPanActive = false;
        if (wasClick && canvasHovered && !overObject && m_hoveredTransitionId == 0) {
            m_contextGraphPosition = ScreenToGraph(mouse, origin);
            ImGui::OpenPopup("GLECanvasContext");
        }
    }

    if (canvasHovered && !overObject && m_hoveredTransitionId == 0 && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
        ClearSelection();
    }

    if (asset.nodes.empty()) {
        const float buttonW = 180.0f;
        const float y = origin.y + canvasSize.y * 0.5f - 22.0f;
        ImGui::SetCursorScreenPos(ImVec2(origin.x + canvasSize.x * 0.5f - buttonW - 6.0f, y));
        if (ImGui::Button("Create Default Loop", ImVec2(buttonW, 0.0f))) DoNewDefault();
        ImGui::SameLine();
        if (ImGui::Button("Add Title Node", ImVec2(buttonW, 0.0f))) AddNodeAt(NodeTemplate::Title, { 0.0f, 0.0f });
    } else {
        const char* hint = "Right-drag pan | Wheel zoom | Drag from output pin to connect | A fit";
        const ImVec2 hintSize = ImGui::CalcTextSize(hint);
        const ImVec2 hintMin(origin.x + 8.0f, origin.y + 8.0f);
        dl->AddRectFilled(ImVec2(hintMin.x - 5.0f, hintMin.y - 3.0f),
            ImVec2(hintMin.x + hintSize.x + 5.0f, hintMin.y + hintSize.y + 3.0f),
            IM_COL32(0, 0, 0, 110), 4.0f);
        dl->AddText(hintMin, IM_COL32(210, 210, 215, 180), hint);
    }

    DrawCanvasContextMenu(origin);
    DrawNodeContextMenu();
    DrawTransitionContextMenu();
    DrawQuickCreatePopup();
}

void GameLoopEditorPanel::DrawGrid(const ImVec2& origin, const ImVec2& canvasSize)
{
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const float gridStep = 32.0f * m_graphZoom;
    if (gridStep <= 6.0f) {
        return;
    }

    const float startX = std::fmod(m_graphOffset.x, gridStep);
    const float startY = std::fmod(m_graphOffset.y, gridStep);
    for (float x = startX; x < canvasSize.x; x += gridStep) {
        dl->AddLine(ImVec2(origin.x + x, origin.y), ImVec2(origin.x + x, origin.y + canvasSize.y), IM_COL32(42, 44, 52, 255));
    }
    for (float y = startY; y < canvasSize.y; y += gridStep) {
        dl->AddLine(ImVec2(origin.x, origin.y + y), ImVec2(origin.x + canvasSize.x, origin.y + y), IM_COL32(42, 44, 52, 255));
    }
}

void GameLoopEditorPanel::DrawTransitionEdge(GameLoopTransition& transition, const ImVec2& origin)
{
    GameLoopAsset& asset = Asset();
    const GameLoopNode* from = asset.FindNode(transition.fromNodeId);
    const GameLoopNode* to = asset.FindNode(transition.toNodeId);
    if (!from || !to) return;

    ImDrawList* dl = ImGui::GetWindowDrawList();
    const ImVec2 fromPos = GraphToScreen(from->graphPos, origin);
    const ImVec2 toPos = GraphToScreen(to->graphPos, origin);
    const ImVec2 start(fromPos.x + kNodeWidth * m_graphZoom, fromPos.y + kNodeHeight * 0.5f * m_graphZoom);
    ImVec2 end(toPos.x, toPos.y + kNodeHeight * 0.5f * m_graphZoom);
    if (transition.fromNodeId == transition.toNodeId) {
        end = ImVec2(fromPos.x + kNodeWidth * m_graphZoom, fromPos.y + 18.0f * m_graphZoom);
    }

    ImVec2 c1(start.x + 120.0f * m_graphZoom, start.y);
    ImVec2 c2(end.x - 120.0f * m_graphZoom, end.y);
    if (transition.fromNodeId == transition.toNodeId) {
        c1 = ImVec2(start.x + 170.0f * m_graphZoom, start.y + 120.0f * m_graphZoom);
        c2 = ImVec2(end.x + 170.0f * m_graphZoom, end.y - 120.0f * m_graphZoom);
    }

    const bool selected = (m_selection == SelectionContext::Transition || m_selection == SelectionContext::Condition) &&
        m_selectedTransitionId == transition.id;
    const bool runtimePending = Runtime().sceneTransitionRequested && Runtime().pendingNodeId == transition.toNodeId;
    ImU32 edgeColor = selected ? IM_COL32(255, 224, 92, 255) : IM_COL32(148, 158, 178, 210);
    if (runtimePending) edgeColor = IM_COL32(115, 220, 145, 255);

    const ImVec2 mouse = ImGui::GetIO().MousePos;
    const bool nearCurve = IsPointNearBezier(mouse, start, c1, c2, end, 12.0f * m_graphZoom);

    ImVec2 dir(end.x - start.x, end.y - start.y);
    float len = std::sqrt(dir.x * dir.x + dir.y * dir.y);
    if (len < 1.0f) len = 1.0f;
    dir.x /= len;
    dir.y /= len;
    const ImVec2 normal(dir.y, -dir.x);

    const int priority = GetSourceLocalPriority(transition);
    const float labelOffset = (42.0f + static_cast<float>(priority) * 16.0f) * m_graphZoom;
    ImVec2 mid = CubicPoint(start, c1, c2, end, 0.5f);
    ImVec2 labelCenter(mid.x + normal.x * labelOffset, mid.y + normal.y * labelOffset);

    const bool compact = m_graphZoom < 0.65f;
    std::string label = BuildTransitionLabel(transition, compact);
    label = BuildMiddleEllipsis(label, 220.0f * m_graphZoom);
    ImVec2 textSize = ImGui::CalcTextSize(label.c_str());
    ImVec2 labelMin(labelCenter.x - textSize.x * 0.5f - 7.0f, labelCenter.y - textSize.y * 0.5f - 4.0f);
    ImVec2 labelMax(labelCenter.x + textSize.x * 0.5f + 7.0f, labelCenter.y + textSize.y * 0.5f + 4.0f);

    const ImVec2 headMin(end.x - 18.0f * m_graphZoom, end.y - 14.0f * m_graphZoom);
    const ImVec2 headMax(end.x + 4.0f * m_graphZoom, end.y + 14.0f * m_graphZoom);
    const bool labelHovered = ImGui::IsMouseHoveringRect(labelMin, labelMax, true);
    const bool headHovered = ImGui::IsMouseHoveringRect(headMin, headMax, true);
    const bool hovered = nearCurve || labelHovered || headHovered;
    if (hovered) {
        m_hoveredTransitionId = transition.id;
        edgeColor = selected ? edgeColor : IM_COL32(210, 220, 245, 255);
    }

    dl->AddBezierCubic(start, c1, c2, end, edgeColor, (selected || hovered) ? 3.0f : 2.0f);
    dl->AddTriangleFilled(
        ImVec2(end.x - 11.0f * m_graphZoom, end.y - 6.0f * m_graphZoom),
        ImVec2(end.x - 11.0f * m_graphZoom, end.y + 6.0f * m_graphZoom),
        end,
        edgeColor);

    dl->AddRectFilled(labelMin, labelMax, IM_COL32(26, 28, 34, 235), 4.0f);
    dl->AddRect(labelMin, labelMax, edgeColor, 4.0f, 0, selected ? 2.0f : 1.0f);
    dl->AddText(ImVec2(labelMin.x + 7.0f, labelMin.y + 4.0f), IM_COL32(238, 238, 238, 255), label.c_str());

    if (!compact && !transition.conditions.empty()) {
        float chipX = labelMin.x;
        const float chipY = labelMax.y + 5.0f;
        const int chipCount = std::min(2, static_cast<int>(transition.conditions.size()));
        for (int i = 0; i < chipCount; ++i) {
            std::string chip = BuildMiddleEllipsis(BuildConditionSummary(transition.conditions[i]), 120.0f);
            ImVec2 chipSize = ImGui::CalcTextSize(chip.c_str());
            ImVec2 chipMin(chipX, chipY);
            ImVec2 chipMax(chipX + chipSize.x + 12.0f, chipY + chipSize.y + 6.0f);
            dl->AddRectFilled(chipMin, chipMax, IM_COL32(44, 48, 58, 225), 4.0f);
            dl->AddText(ImVec2(chipMin.x + 6.0f, chipMin.y + 3.0f), IM_COL32(206, 216, 230, 255), chip.c_str());
            chipX = chipMax.x + 5.0f;
        }
        if (static_cast<int>(transition.conditions.size()) > chipCount) {
            char more[16];
            std::snprintf(more, sizeof(more), "+%d", static_cast<int>(transition.conditions.size()) - chipCount);
            dl->AddText(ImVec2(chipX + 2.0f, chipY + 3.0f), IM_COL32(220, 220, 150, 255), more);
        }
    }
}

void GameLoopEditorPanel::DrawNode(GameLoopNode& node, const ImVec2& origin)
{
    GameLoopAsset& asset = Asset();
    const GameLoopRuntime& runtime = Runtime();
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const ImVec2 nodePos = GraphToScreen(node.graphPos, origin);
    const ImVec2 nodeSize(kNodeWidth * m_graphZoom, kNodeHeight * m_graphZoom);
    const ImVec2 nodeEnd(nodePos.x + nodeSize.x, nodePos.y + nodeSize.y);
    const ImVec2 inputPin(nodePos.x, nodePos.y + nodeSize.y * 0.5f);
    const ImVec2 outputPin(nodeEnd.x, nodePos.y + nodeSize.y * 0.5f);

    const bool selected = m_selection == SelectionContext::Node && m_selectedNodeId == node.id;
    const bool isStart = asset.startNodeId == node.id;
    const bool isCurrent = runtime.isActive && runtime.currentNodeId == node.id;
    const bool isPending = runtime.sceneTransitionRequested && runtime.pendingNodeId == node.id;
    const int severity = NodePathSeverity(node);

    ImU32 fill = selected ? IM_COL32(58, 67, 86, 255) : IM_COL32(45, 49, 58, 255);
    ImU32 border = IM_COL32(104, 112, 130, 255);
    if (isStart) border = IM_COL32(90, 150, 255, 255);
    if (isCurrent) border = IM_COL32(88, 220, 126, 255);
    if (isPending) border = IM_COL32(255, 208, 80, 255);

    dl->AddRectFilled(ImVec2(nodePos.x + 3.0f, nodePos.y + 3.0f), ImVec2(nodeEnd.x + 3.0f, nodeEnd.y + 3.0f), IM_COL32(0, 0, 0, 80), 8.0f * m_graphZoom);
    dl->AddRectFilled(nodePos, nodeEnd, fill, 8.0f * m_graphZoom);
    dl->AddRect(nodePos, nodeEnd, border, 8.0f * m_graphZoom, 0, selected ? 3.0f : 2.0f);

    const float pinR = std::max(4.0f, kPinRadius * m_graphZoom);
    dl->AddCircleFilled(inputPin, pinR, IM_COL32(118, 168, 235, 255));
    dl->AddCircleFilled(outputPin, pinR, IM_COL32(238, 176, 84, 255));

    const char* badge = SeverityText(severity);
    const ImVec2 badgeSize = ImGui::CalcTextSize(badge);
    const ImVec2 badgeMin(nodeEnd.x - badgeSize.x - 18.0f * m_graphZoom, nodePos.y + 8.0f * m_graphZoom);
    const ImVec2 badgeMax(nodeEnd.x - 8.0f * m_graphZoom, nodePos.y + 28.0f * m_graphZoom);
    dl->AddRectFilled(badgeMin, badgeMax, SeverityColor(severity), 4.0f * m_graphZoom);
    dl->AddText(ImVec2(badgeMin.x + 6.0f, badgeMin.y + 3.0f), IM_COL32(20, 22, 26, 255), badge);

    dl->PushClipRect(ImVec2(nodePos.x + 12.0f * m_graphZoom, nodePos.y), ImVec2(badgeMin.x - 6.0f, nodeEnd.y), true);
    std::string title = BuildMiddleEllipsis(node.name, (badgeMin.x - nodePos.x) - 24.0f * m_graphZoom);
    dl->AddText(ImVec2(nodePos.x + 12.0f * m_graphZoom, nodePos.y + 10.0f * m_graphZoom), IM_COL32(255, 255, 255, 255), title.c_str());
    if (m_graphZoom >= 0.55f) {
        std::string path = BuildMiddleEllipsis(node.scenePath, nodeSize.x - 26.0f * m_graphZoom);
        dl->AddText(ImVec2(nodePos.x + 12.0f * m_graphZoom, nodePos.y + 35.0f * m_graphZoom), IM_COL32(178, 185, 198, 255), path.c_str());
    }
    dl->PopClipRect();

    if (m_graphZoom >= 0.45f) {
        if (isStart) dl->AddText(ImVec2(nodePos.x + 12.0f * m_graphZoom, nodePos.y + 62.0f * m_graphZoom), IM_COL32(130, 176, 255, 255), "START");
        if (isCurrent) dl->AddText(ImVec2(nodeEnd.x - 76.0f * m_graphZoom, nodePos.y + 62.0f * m_graphZoom), IM_COL32(120, 255, 150, 255), "CURRENT");
    }

    ImGui::SetCursorScreenPos(ImVec2(outputPin.x - 12.0f * m_graphZoom, outputPin.y - 12.0f * m_graphZoom));
    ImGui::InvisibleButton(("out_pin##" + std::to_string(node.id)).c_str(), ImVec2(24.0f * m_graphZoom, 24.0f * m_graphZoom));
    if (ImGui::IsItemHovered()) {
        m_hoveredOutputNodeId = node.id;
        dl->AddCircle(outputPin, pinR + 5.0f, IM_COL32(255, 224, 120, 255), 0, 2.0f);
        ImGui::SetTooltip("Drag to a node or empty canvas to create transition");
    }
    if (ImGui::IsItemActivated()) {
        m_isConnecting = true;
        m_connectFromNodeId = node.id;
        m_connectionDragged = false;
    }

    ImGui::SetCursorScreenPos(ImVec2(inputPin.x - 12.0f * m_graphZoom, inputPin.y - 12.0f * m_graphZoom));
    ImGui::InvisibleButton(("in_pin##" + std::to_string(node.id)).c_str(), ImVec2(24.0f * m_graphZoom, 24.0f * m_graphZoom));
    if (ImGui::IsItemHovered()) {
        m_hoveredInputNodeId = node.id;
        dl->AddCircle(inputPin, pinR + 5.0f, m_isConnecting ? IM_COL32(100, 245, 145, 255) : IM_COL32(150, 195, 255, 255), 0, 2.0f);
    }

    ImGui::SetCursorScreenPos(nodePos);
    ImGui::InvisibleButton(("node_body##" + std::to_string(node.id)).c_str(), nodeSize);
    const bool bodyHovered = ImGui::IsItemHovered();
    if (bodyHovered) {
        m_hoveredNodeId = node.id;
        if (m_isConnecting) {
            dl->AddRect(nodePos, nodeEnd, IM_COL32(90, 240, 140, 255), 8.0f * m_graphZoom, 0, 3.0f);
        }
    }
    if (ImGui::IsItemClicked(ImGuiMouseButton_Left) && !m_isConnecting) {
        SelectNode(node.id);
    }
    if (bodyHovered && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
        const float localY = ImGui::GetIO().MousePos.y - nodePos.y;
        if (localY < 32.0f * m_graphZoom) StartNodeNameEdit(node);
        else StartNodePathEdit(node);
    }
    if (ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Left, 0.0f) &&
        !m_isConnecting && m_inlineNameNodeId != node.id && m_inlinePathNodeId != node.id) {
        const ImVec2 delta = ImGui::GetIO().MouseDelta;
        node.graphPos.x += delta.x / m_graphZoom;
        node.graphPos.y += delta.y / m_graphZoom;
        m_dirty = true;
    }
    if (m_isConnecting && bodyHovered && ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
        AddTransition(m_connectFromNodeId, node.id, true);
        m_isConnecting = false;
        m_connectFromNodeId = 0;
        m_connectionDragged = false;
    }
    if (ImGui::IsItemClicked(ImGuiMouseButton_Right)) {
        SelectNode(node.id);
        ImGui::OpenPopup("GLENodeContext");
    }

    DrawNodeInlineEditors(node, nodePos, nodeSize);
}

void GameLoopEditorPanel::DrawCanvasContextMenu(const ImVec2& origin)
{
    (void)origin;
    if (!ImGui::BeginPopup("GLECanvasContext")) return;
    if (ImGui::MenuItem("Title")) AddNodeAt(NodeTemplate::Title, m_contextGraphPosition);
    if (ImGui::MenuItem("Battle")) AddNodeAt(NodeTemplate::Battle, m_contextGraphPosition);
    if (ImGui::MenuItem("Result")) AddNodeAt(NodeTemplate::Result, m_contextGraphPosition);
    if (ImGui::MenuItem("Custom Scene")) AddNodeAt(NodeTemplate::Custom, m_contextGraphPosition);
    ImGui::Separator();
    if (ImGui::MenuItem("Create Default Loop")) DoNewDefault();
    if (ImGui::MenuItem("Fit Graph")) RequestFitGraph();
    ImGui::EndPopup();
}

void GameLoopEditorPanel::DrawNodeContextMenu()
{
    if (!ImGui::BeginPopup("GLENodeContext")) return;
    GameLoopNode* node = FindSelectedNode();
    if (!node) {
        ImGui::EndPopup();
        return;
    }
    if (ImGui::MenuItem("Rename")) StartNodeNameEdit(*node);
    if (ImGui::MenuItem("Edit Scene Path")) StartNodePathEdit(*node);
    if (ImGui::MenuItem("Set As Start")) {
        Asset().startNodeId = node->id;
        m_dirty = true;
    }
    if (ImGui::MenuItem("Connect From Here")) {
        m_isConnecting = true;
        m_connectFromNodeId = node->id;
        m_connectionDragged = false;
    }
    if (ImGui::MenuItem("Frame Selected")) RequestFitGraph();
    ImGui::Separator();
    if (ImGui::MenuItem("Duplicate")) DuplicateSelected();
    if (ImGui::MenuItem("Delete")) DeleteSelectedNode();
    ImGui::EndPopup();
}

void GameLoopEditorPanel::DrawTransitionContextMenu()
{
    if (!ImGui::BeginPopup("GLETransitionContext")) return;
    GameLoopTransition* transition = FindSelectedTransition();
    if (!transition) {
        ImGui::EndPopup();
        return;
    }
    if (ImGui::MenuItem("Edit")) OpenFloatingTransitionEditor(transition->id, ImGui::GetIO().MousePos);
    if (ImGui::BeginMenu("Add Condition")) {
        const char* presets[] = {
            "Input Confirm", "Input Cancel", "Input Retry", "Timer 3s",
            "Player Moved 1.0", "Enemy Dead", "All Enemies Dead", "Runtime Flag", "UI Button"
        };
        for (int i = 0; i < static_cast<int>(sizeof(presets) / sizeof(presets[0])); ++i) {
            if (ImGui::MenuItem(presets[i])) AddConditionPreset(*transition, i);
        }
        ImGui::EndMenu();
    }
    if (ImGui::MenuItem("Priority Up")) MoveSelectedTransitionSourceLocal(-1);
    if (ImGui::MenuItem("Priority Down")) MoveSelectedTransitionSourceLocal(1);
    if (ImGui::MenuItem("Reverse")) {
        std::swap(transition->fromNodeId, transition->toNodeId);
        m_dirty = true;
    }
    if (ImGui::BeginMenu("Loading")) {
        if (ImGui::MenuItem("Immediate")) transition->loadingPolicy.mode = GameLoopLoadingMode::Immediate;
        if (ImGui::MenuItem("Fade")) transition->loadingPolicy.mode = GameLoopLoadingMode::FadeOnly;
        if (ImGui::MenuItem("Overlay")) transition->loadingPolicy.mode = GameLoopLoadingMode::LoadingOverlay;
        m_dirty = true;
        ImGui::EndMenu();
    }
    ImGui::Separator();
    if (ImGui::MenuItem("Delete")) DeleteSelectedTransition();
    ImGui::EndPopup();
}

void GameLoopEditorPanel::DrawQuickCreatePopup()
{
    if (!ImGui::BeginPopup("GLEQuickCreateNode")) return;
    auto createAndConnect = [&](NodeTemplate nodeTemplate) {
        GameLoopNode& node = AddNodeAt(nodeTemplate, m_quickCreateGraphPos);
        AddTransition(m_quickCreateFromNodeId, node.id, true);
        m_quickCreateFromNodeId = 0;
    };
    if (ImGui::MenuItem("Title")) createAndConnect(NodeTemplate::Title);
    if (ImGui::MenuItem("Battle")) createAndConnect(NodeTemplate::Battle);
    if (ImGui::MenuItem("Result")) createAndConnect(NodeTemplate::Result);
    if (ImGui::MenuItem("Custom Scene")) createAndConnect(NodeTemplate::Custom);
    ImGui::EndPopup();
}

void GameLoopEditorPanel::DrawFloatingTransitionEditor()
{
    if (!m_floatingTransitionEditorOpen) return;
    GameLoopTransition* transition = FindTransitionById(m_floatingTransitionId);
    if (!transition) {
        m_floatingTransitionEditorOpen = false;
        return;
    }

    ImGui::SetNextWindowPos(ImVec2(m_floatingEditorScreenPos.x, m_floatingEditorScreenPos.y), ImGuiCond_Appearing);
    ImGui::SetNextWindowSize(ImVec2(420.0f, 520.0f), ImGuiCond_Appearing);
    bool open = true;
    if (ImGui::Begin("Transition Editor##GLEFloating", &open, ImGuiWindowFlags_NoSavedSettings)) {
        DrawTransitionProperties(*transition);
    }
    ImGui::End();
    m_floatingTransitionEditorOpen = open;
}

void GameLoopEditorPanel::DrawNodeProperties(GameLoopNode& node)
{
    char nameBuf[128] = {};
    std::strncpy(nameBuf, node.name.c_str(), sizeof(nameBuf) - 1);
    if (ImGui::InputText("Name", nameBuf, sizeof(nameBuf))) {
        node.name = nameBuf;
        m_dirty = true;
    }

    char pathBuf[256] = {};
    std::strncpy(pathBuf, node.scenePath.c_str(), sizeof(pathBuf) - 1);
    if (ImGui::InputText("Scene Path", pathBuf, sizeof(pathBuf))) {
        const std::string normalized = NormalizeGameLoopScenePath(pathBuf);
        node.scenePath = normalized.empty() ? std::string(pathBuf) : normalized;
        m_dirty = true;
    }

    ImGui::Text("Id: %u", node.id);
    ImGui::Text("Path Status: %s", SeverityText(NodePathSeverity(node)));
    const bool isStart = Asset().startNodeId == node.id;
    ImGui::Text("Start Node: %s", isStart ? "Yes" : "No");
    if (!isStart && ImGui::Button("Set As Start")) {
        Asset().startNodeId = node.id;
        m_dirty = true;
    }
    ImGui::Separator();
    if (ImGui::Button("Duplicate")) DuplicateSelected();
    ImGui::SameLine();
    if (ImGui::Button("Delete")) DeleteSelectedNode();
}

void GameLoopEditorPanel::DrawTransitionProperties(GameLoopTransition& transition)
{
    GameLoopAsset& asset = Asset();
    const GameLoopNode* from = asset.FindNode(transition.fromNodeId);
    const GameLoopNode* to = asset.FindNode(transition.toNodeId);
    ImGui::Text("%s -> %s", from ? from->name.c_str() : "?", to ? to->name.c_str() : "?");
    ImGui::Text("Id: %u  Priority: #%d", transition.id, GetSourceLocalPriority(transition));

    char nameBuf[128] = {};
    std::strncpy(nameBuf, transition.name.c_str(), sizeof(nameBuf) - 1);
    if (ImGui::InputText("Name", nameBuf, sizeof(nameBuf))) {
        transition.name = nameBuf;
        m_dirty = true;
    }

    int mode = transition.requireAllConditions ? 1 : 0;
    const char* modeLabels[] = { "Any", "All" };
    if (ImGui::Combo("Condition Mode", &mode, modeLabels, 2)) {
        transition.requireAllConditions = (mode == 1);
        m_dirty = true;
    }

    if (ImGui::Button("Priority Up")) MoveSelectedTransitionSourceLocal(-1);
    ImGui::SameLine();
    if (ImGui::Button("Priority Down")) MoveSelectedTransitionSourceLocal(1);
    ImGui::SameLine();
    if (ImGui::Button("Reverse")) {
        std::swap(transition.fromNodeId, transition.toNodeId);
        m_dirty = true;
    }

    DrawLoadingPolicyEditor(transition.loadingPolicy);
    DrawConditionList(transition);
}

void GameLoopEditorPanel::DrawLoadingPolicyEditor(GameLoopLoadingPolicy& policy)
{
    if (!ImGui::CollapsingHeader("Loading", ImGuiTreeNodeFlags_DefaultOpen)) return;
    int modeIndex = FindLoadingIndex(policy.mode);
    if (ImGui::Combo("Mode", &modeIndex, kLoadingLabels, kLoadingCount)) {
        policy.mode = kLoadingValues[modeIndex];
        m_dirty = true;
    }
    if (ImGui::InputFloat("Fade Out", &policy.fadeOutSeconds)) m_dirty = true;
    if (ImGui::InputFloat("Fade In", &policy.fadeInSeconds)) m_dirty = true;
    if (ImGui::InputFloat("Minimum", &policy.minimumLoadingSeconds)) m_dirty = true;
    char messageBuf[192] = {};
    std::strncpy(messageBuf, policy.loadingMessage.c_str(), sizeof(messageBuf) - 1);
    if (ImGui::InputText("Message", messageBuf, sizeof(messageBuf))) {
        policy.loadingMessage = messageBuf;
        m_dirty = true;
    }
    if (ImGui::Checkbox("Block Input", &policy.blockInput)) m_dirty = true;
}

void GameLoopEditorPanel::DrawConditionList(GameLoopTransition& transition)
{
    if (!ImGui::CollapsingHeader("Conditions", ImGuiTreeNodeFlags_DefaultOpen)) return;
    if (ImGui::Button("Add Condition")) ImGui::OpenPopup("GLEAddCondition");
    if (ImGui::BeginPopup("GLEAddCondition")) {
        const char* presets[] = {
            "Input Confirm", "Input Cancel", "Input Retry", "Timer 3s",
            "Player Moved 1.0", "Enemy Dead", "All Enemies Dead", "Runtime Flag", "UI Button"
        };
        for (int i = 0; i < static_cast<int>(sizeof(presets) / sizeof(presets[0])); ++i) {
            if (ImGui::MenuItem(presets[i])) AddConditionPreset(transition, i);
        }
        ImGui::EndPopup();
    }

    int removeAt = -1;
    ImGui::BeginChild("GLEConditionList", ImVec2(0.0f, 120.0f), true);
    for (int i = 0; i < static_cast<int>(transition.conditions.size()); ++i) {
        ImGui::PushID(i);
        if (ImGui::Selectable(BuildConditionSummary(transition.conditions[i]).c_str(), m_selectedConditionIndex == i)) {
            m_selection = SelectionContext::Condition;
            m_selectedConditionIndex = i;
        }
        ImGui::SameLine();
        if (ImGui::SmallButton("X")) removeAt = i;
        ImGui::PopID();
    }
    ImGui::EndChild();

    if (removeAt >= 0) {
        transition.conditions.erase(transition.conditions.begin() + removeAt);
        if (m_selectedConditionIndex >= static_cast<int>(transition.conditions.size())) {
            m_selectedConditionIndex = static_cast<int>(transition.conditions.size()) - 1;
        }
        m_dirty = true;
    }

    if (m_selectedConditionIndex >= 0 && m_selectedConditionIndex < static_cast<int>(transition.conditions.size())) {
        DrawConditionEditor(transition.conditions[m_selectedConditionIndex]);
    }
}

void GameLoopEditorPanel::DrawConditionEditor(GameLoopCondition& condition)
{
    int typeIndex = FindConditionIndex(condition.type);
    if (ImGui::Combo("Type", &typeIndex, kConditionLabels, kConditionCount)) {
        condition.type = kConditionValues[typeIndex];
        m_dirty = true;
    }

    switch (condition.type) {
    case GameLoopConditionType::InputPressed:
    {
        int actionIndex = std::max(0, std::min(condition.actionIndex, kActionCount - 1));
        if (ImGui::Combo("Action", &actionIndex, kActionLabels, kActionCount)) {
            condition.actionIndex = actionIndex;
            m_dirty = true;
        }
        break;
    }
    case GameLoopConditionType::TimerElapsed:
        if (ImGui::InputFloat("Seconds", &condition.seconds)) m_dirty = true;
        break;
    case GameLoopConditionType::ActorDead:
    case GameLoopConditionType::AllActorsDead:
    {
        int actorIndex = FindActorIndex(condition.actorType);
        if (ImGui::Combo("Actor", &actorIndex, kActorLabels, kActorCount)) {
            condition.actorType = kActorValues[actorIndex];
            m_dirty = true;
        }
        break;
    }
    case GameLoopConditionType::ActorMovedDistance:
    {
        int actorIndex = FindActorIndex(condition.actorType);
        if (ImGui::Combo("Actor", &actorIndex, kActorLabels, kActorCount)) {
            condition.actorType = kActorValues[actorIndex];
            m_dirty = true;
        }
        if (ImGui::InputFloat("Distance", &condition.threshold)) m_dirty = true;
        break;
    }
    case GameLoopConditionType::UIButtonClicked:
    {
        char buf[128] = {};
        std::strncpy(buf, condition.targetName.c_str(), sizeof(buf) - 1);
        if (ImGui::InputText("Button Id", buf, sizeof(buf))) {
            condition.targetName = buf;
            m_dirty = true;
        }
        break;
    }
    case GameLoopConditionType::RuntimeFlag:
    case GameLoopConditionType::StateMachineState:
    {
        char buf[128] = {};
        std::strncpy(buf, condition.parameterName.c_str(), sizeof(buf) - 1);
        if (ImGui::InputText("Parameter", buf, sizeof(buf))) {
            condition.parameterName = buf;
            m_dirty = true;
        }
        break;
    }
    case GameLoopConditionType::TimelineEvent:
    case GameLoopConditionType::CustomEvent:
    {
        char buf[128] = {};
        std::strncpy(buf, condition.eventName.c_str(), sizeof(buf) - 1);
        if (ImGui::InputText("Event", buf, sizeof(buf))) {
            condition.eventName = buf;
            m_dirty = true;
        }
        break;
    }
    default:
        ImGui::TextDisabled("No parameters.");
        break;
    }
}

GameLoopNode* GameLoopEditorPanel::FindSelectedNode()
{
    return m_selectedNodeId == 0 ? nullptr : Asset().FindNode(m_selectedNodeId);
}

GameLoopTransition* GameLoopEditorPanel::FindSelectedTransition()
{
    return FindTransitionById(m_selectedTransitionId);
}

GameLoopTransition* GameLoopEditorPanel::FindTransitionById(uint32_t transitionId)
{
    if (transitionId == 0) return nullptr;
    for (GameLoopTransition& transition : Asset().transitions) {
        if (transition.id == transitionId) return &transition;
    }
    return nullptr;
}

int GameLoopEditorPanel::FindSelectedTransitionIndex() const
{
    const GameLoopAsset& asset = Asset();
    for (int i = 0; i < static_cast<int>(asset.transitions.size()); ++i) {
        if (asset.transitions[i].id == m_selectedTransitionId) return i;
    }
    return -1;
}

ImVec2 GameLoopEditorPanel::GraphToScreen(const DirectX::XMFLOAT2& graphPos, const ImVec2& origin) const
{
    return ImVec2(origin.x + m_graphOffset.x + graphPos.x * m_graphZoom,
                  origin.y + m_graphOffset.y + graphPos.y * m_graphZoom);
}

DirectX::XMFLOAT2 GameLoopEditorPanel::ScreenToGraph(const ImVec2& screenPos, const ImVec2& origin) const
{
    return {
        (screenPos.x - origin.x - m_graphOffset.x) / m_graphZoom,
        (screenPos.y - origin.y - m_graphOffset.y) / m_graphZoom
    };
}

void GameLoopEditorPanel::SelectNode(uint32_t nodeId)
{
    m_selection = SelectionContext::Node;
    m_selectedNodeId = nodeId;
    m_selectedTransitionId = 0;
    m_selectedConditionIndex = -1;
    m_floatingTransitionEditorOpen = false;
}

void GameLoopEditorPanel::SelectTransition(uint32_t transitionId)
{
    m_selection = SelectionContext::Transition;
    m_selectedTransitionId = transitionId;
    m_selectedNodeId = 0;
    m_selectedConditionIndex = -1;
}

void GameLoopEditorPanel::ClearSelection()
{
    m_selection = SelectionContext::None;
    m_selectedNodeId = 0;
    m_selectedTransitionId = 0;
    m_selectedConditionIndex = -1;
}

GameLoopNode& GameLoopEditorPanel::AddNodeAt(NodeTemplate nodeTemplate, const DirectX::XMFLOAT2& requestedPosition)
{
    GameLoopAsset& asset = Asset();
    GameLoopNode node;
    node.id = asset.AllocateNodeId();
    node.type = GameLoopNodeType::Scene;
    node.graphPos = FindOpenGraphPosition(requestedPosition);

    switch (nodeTemplate) {
    case NodeTemplate::Title:
        node.name = "Title";
        node.scenePath = "Data/Scenes/Title.scene";
        break;
    case NodeTemplate::Battle:
        node.name = "Battle";
        node.scenePath = "Data/Scenes/Battle.scene";
        break;
    case NodeTemplate::Result:
        node.name = "Result";
        node.scenePath = "Data/Scenes/Result.scene";
        break;
    default:
        node.name = "Scene";
        node.scenePath = "Data/Scenes/New.scene";
        break;
    }

    if (asset.nodes.empty()) asset.startNodeId = node.id;
    asset.nodes.push_back(node);
    GameLoopNode& created = asset.nodes.back();
    SelectNode(created.id);
    StartNodeNameEdit(created);
    m_dirty = true;
    return created;
}

void GameLoopEditorPanel::AddTransition(uint32_t fromNodeId, uint32_t toNodeId, bool openEditor)
{
    GameLoopAsset& asset = Asset();
    if (fromNodeId == 0 || toNodeId == 0) return;

    GameLoopTransition transition;
    transition.id = asset.AllocateTransitionId();
    transition.fromNodeId = fromNodeId;
    transition.toNodeId = toNodeId;
    transition.requireAllConditions = true;
    const GameLoopNode* from = asset.FindNode(fromNodeId);
    const GameLoopNode* to = asset.FindNode(toNodeId);
    transition.name = (from && to) ? (from->name + "To" + to->name) : "Transition";
    ApplyDefaultCondition(transition);
    asset.transitions.push_back(transition);
    SelectTransition(transition.id);
    m_selectedConditionIndex = transition.conditions.empty() ? -1 : 0;
    m_dirty = true;
    if (openEditor) OpenFloatingTransitionEditor(transition.id, ImGui::GetIO().MousePos);
}

void GameLoopEditorPanel::ApplyDefaultCondition(GameLoopTransition& transition)
{
    const GameLoopAsset& asset = Asset();
    const GameLoopNode* from = asset.FindNode(transition.fromNodeId);
    const GameLoopNode* to = asset.FindNode(transition.toNodeId);
    GameLoopCondition condition;
    if (from && to && from->name == "Title" && to->name == "Battle") {
        condition.type = GameLoopConditionType::InputPressed;
        condition.actionIndex = 0;
    } else if (from && to && from->name == "Result" && to->name == "Battle") {
        condition.type = GameLoopConditionType::InputPressed;
        condition.actionIndex = 2;
    } else if (from && to && from->name == "Result" && to->name == "Title") {
        condition.type = GameLoopConditionType::InputPressed;
        condition.actionIndex = 1;
    } else if (from && to && from->name == "Battle" && to->name == "Result") {
        condition.type = GameLoopConditionType::ActorMovedDistance;
        condition.actorType = ActorType::Player;
        condition.threshold = 1.0f;
    } else {
        condition.type = GameLoopConditionType::InputPressed;
        condition.actionIndex = 0;
    }
    transition.conditions.push_back(condition);
}

void GameLoopEditorPanel::AddConditionPreset(GameLoopTransition& transition, int presetIndex)
{
    GameLoopCondition condition;
    switch (presetIndex) {
    case 0:
        condition.type = GameLoopConditionType::InputPressed;
        condition.actionIndex = 0;
        break;
    case 1:
        condition.type = GameLoopConditionType::InputPressed;
        condition.actionIndex = 1;
        break;
    case 2:
        condition.type = GameLoopConditionType::InputPressed;
        condition.actionIndex = 2;
        break;
    case 3:
        condition.type = GameLoopConditionType::TimerElapsed;
        condition.seconds = 3.0f;
        break;
    case 4:
        condition.type = GameLoopConditionType::ActorMovedDistance;
        condition.actorType = ActorType::Player;
        condition.threshold = 1.0f;
        break;
    case 5:
        condition.type = GameLoopConditionType::ActorDead;
        condition.actorType = ActorType::Enemy;
        break;
    case 6:
        condition.type = GameLoopConditionType::AllActorsDead;
        condition.actorType = ActorType::Enemy;
        break;
    case 7:
        condition.type = GameLoopConditionType::RuntimeFlag;
        condition.parameterName = "FlagName";
        break;
    case 8:
        condition.type = GameLoopConditionType::UIButtonClicked;
        condition.targetName = "ButtonId";
        break;
    default:
        condition.type = GameLoopConditionType::InputPressed;
        condition.actionIndex = 0;
        break;
    }
    transition.conditions.push_back(condition);
    m_selectedConditionIndex = static_cast<int>(transition.conditions.size()) - 1;
    m_selection = SelectionContext::Condition;
    m_dirty = true;
}

void GameLoopEditorPanel::DeleteSelected()
{
    if (m_selection == SelectionContext::Node) DeleteSelectedNode();
    else if (m_selection == SelectionContext::Transition || m_selection == SelectionContext::Condition) DeleteSelectedTransition();
}

void GameLoopEditorPanel::DeleteSelectedNode()
{
    GameLoopAsset& asset = Asset();
    const uint32_t nodeId = m_selectedNodeId;
    if (nodeId == 0) return;
    asset.nodes.erase(std::remove_if(asset.nodes.begin(), asset.nodes.end(),
        [nodeId](const GameLoopNode& node) { return node.id == nodeId; }), asset.nodes.end());
    asset.transitions.erase(std::remove_if(asset.transitions.begin(), asset.transitions.end(),
        [nodeId](const GameLoopTransition& transition) {
            return transition.fromNodeId == nodeId || transition.toNodeId == nodeId;
        }), asset.transitions.end());
    if (asset.startNodeId == nodeId) {
        asset.startNodeId = asset.nodes.empty() ? 0u : asset.nodes.front().id;
    }
    ClearSelection();
    m_dirty = true;
}

void GameLoopEditorPanel::DeleteSelectedTransition()
{
    GameLoopAsset& asset = Asset();
    const uint32_t transitionId = m_selectedTransitionId;
    if (transitionId == 0) return;
    asset.transitions.erase(std::remove_if(asset.transitions.begin(), asset.transitions.end(),
        [transitionId](const GameLoopTransition& transition) { return transition.id == transitionId; }), asset.transitions.end());
    ClearSelection();
    if (m_floatingTransitionId == transitionId) {
        m_floatingTransitionEditorOpen = false;
        m_floatingTransitionId = 0;
    }
    m_dirty = true;
}

void GameLoopEditorPanel::DuplicateSelected()
{
    if (m_selection == SelectionContext::Node) {
        GameLoopNode* node = FindSelectedNode();
        if (!node) return;
        GameLoopNode duplicate = *node;
        duplicate.id = Asset().AllocateNodeId();
        duplicate.name += " Copy";
        duplicate.graphPos = FindOpenGraphPosition({ node->graphPos.x + 44.0f, node->graphPos.y + 44.0f });
        Asset().nodes.push_back(duplicate);
        SelectNode(duplicate.id);
        m_dirty = true;
    } else if (m_selection == SelectionContext::Transition || m_selection == SelectionContext::Condition) {
        const int index = FindSelectedTransitionIndex();
        if (index < 0) return;
        GameLoopTransition copy = Asset().transitions[index];
        copy.id = Asset().AllocateTransitionId();
        copy.name += " Copy";
        Asset().transitions.insert(Asset().transitions.begin() + index + 1, copy);
        SelectTransition(copy.id);
        m_dirty = true;
    }
}

void GameLoopEditorPanel::MoveSelectedTransitionSourceLocal(int direction)
{
    if (direction == 0) return;
    GameLoopAsset& asset = Asset();
    const int selectedIndex = FindSelectedTransitionIndex();
    if (selectedIndex < 0) return;

    const uint32_t source = asset.transitions[selectedIndex].fromNodeId;
    std::vector<int> sourceIndices;
    for (int i = 0; i < static_cast<int>(asset.transitions.size()); ++i) {
        if (asset.transitions[i].fromNodeId == source) sourceIndices.push_back(i);
    }
    auto it = std::find(sourceIndices.begin(), sourceIndices.end(), selectedIndex);
    if (it == sourceIndices.end()) return;
    int localIndex = static_cast<int>(std::distance(sourceIndices.begin(), it));
    int targetLocal = localIndex + (direction < 0 ? -1 : 1);
    if (targetLocal < 0 || targetLocal >= static_cast<int>(sourceIndices.size())) return;
    std::swap(asset.transitions[selectedIndex], asset.transitions[sourceIndices[targetLocal]]);
    m_dirty = true;
}

int GameLoopEditorPanel::GetSourceLocalPriority(const GameLoopTransition& transition) const
{
    int priority = 0;
    for (const GameLoopTransition& candidate : Asset().transitions) {
        if (candidate.fromNodeId != transition.fromNodeId) continue;
        if (candidate.id == transition.id) return priority;
        ++priority;
    }
    return 0;
}

void GameLoopEditorPanel::RequestFitGraph()
{
    m_graphFitRequested = true;
}

void GameLoopEditorPanel::FitGraphToContent(const ImVec2& canvasSize)
{
    const GameLoopAsset& asset = Asset();
    if (asset.nodes.empty() || canvasSize.x <= 0.0f || canvasSize.y <= 0.0f) {
        m_graphZoom = 1.0f;
        m_graphOffset = { canvasSize.x * 0.5f, canvasSize.y * 0.5f };
        return;
    }

    float minX = FLT_MAX;
    float minY = FLT_MAX;
    float maxX = -FLT_MAX;
    float maxY = -FLT_MAX;
    for (const GameLoopNode& node : asset.nodes) {
        minX = std::min(minX, node.graphPos.x);
        minY = std::min(minY, node.graphPos.y);
        maxX = std::max(maxX, node.graphPos.x + kNodeWidth);
        maxY = std::max(maxY, node.graphPos.y + kNodeHeight);
    }

    const float contentW = std::max(1.0f, maxX - minX);
    const float contentH = std::max(1.0f, maxY - minY);
    const float zoomX = (canvasSize.x - kFitPadding * 2.0f) / contentW;
    const float zoomY = (canvasSize.y - kFitPadding * 2.0f) / contentH;
    m_graphZoom = ClampF(std::min(zoomX, zoomY), 0.35f, 1.35f);
    m_graphOffset.x = kFitPadding - minX * m_graphZoom + (canvasSize.x - kFitPadding * 2.0f - contentW * m_graphZoom) * 0.5f;
    m_graphOffset.y = kFitPadding - minY * m_graphZoom + (canvasSize.y - kFitPadding * 2.0f - contentH * m_graphZoom) * 0.5f;
}

void GameLoopEditorPanel::FrameSelected(const ImVec2& canvasSize)
{
    const GameLoopNode* node = nullptr;
    if (m_selection == SelectionContext::Node) {
        node = Asset().FindNode(m_selectedNodeId);
    } else if (const GameLoopTransition* transition = FindSelectedTransition()) {
        node = Asset().FindNode(transition->fromNodeId);
    }
    if (!node) {
        RequestFitGraph();
        return;
    }
    m_graphOffset.x = canvasSize.x * 0.5f - (node->graphPos.x + kNodeWidth * 0.5f) * m_graphZoom;
    m_graphOffset.y = canvasSize.y * 0.5f - (node->graphPos.y + kNodeHeight * 0.5f) * m_graphZoom;
}

DirectX::XMFLOAT2 GameLoopEditorPanel::FindOpenGraphPosition(const DirectX::XMFLOAT2& requestedPosition) const
{
    DirectX::XMFLOAT2 candidate = requestedPosition;
    for (int attempt = 0; attempt < 32; ++attempt) {
        bool overlaps = false;
        for (const GameLoopNode& node : Asset().nodes) {
            const bool xOverlap = candidate.x < node.graphPos.x + kNodeWidth + 24.0f && candidate.x + kNodeWidth + 24.0f > node.graphPos.x;
            const bool yOverlap = candidate.y < node.graphPos.y + kNodeHeight + 24.0f && candidate.y + kNodeHeight + 24.0f > node.graphPos.y;
            if (xOverlap && yOverlap) {
                overlaps = true;
                break;
            }
        }
        if (!overlaps) return candidate;
        const int ring = attempt / 8 + 1;
        const int side = attempt % 8;
        candidate.x = requestedPosition.x + static_cast<float>((side % 4) - 1) * 320.0f * ring;
        candidate.y = requestedPosition.y + static_cast<float>((side / 4) * 2 - 1) * 150.0f * ring;
    }
    return candidate;
}

void GameLoopEditorPanel::StartNodeNameEdit(const GameLoopNode& node)
{
    m_inlineNameNodeId = node.id;
    m_inlinePathNodeId = 0;
    std::memset(m_inlineNameBuf, 0, sizeof(m_inlineNameBuf));
    std::strncpy(m_inlineNameBuf, node.name.c_str(), sizeof(m_inlineNameBuf) - 1);
    m_focusInlineEditor = true;
}

void GameLoopEditorPanel::StartNodePathEdit(const GameLoopNode& node)
{
    m_inlinePathNodeId = node.id;
    m_inlineNameNodeId = 0;
    std::memset(m_inlinePathBuf, 0, sizeof(m_inlinePathBuf));
    std::strncpy(m_inlinePathBuf, node.scenePath.c_str(), sizeof(m_inlinePathBuf) - 1);
    m_focusInlineEditor = true;
}

void GameLoopEditorPanel::DrawNodeInlineEditors(GameLoopNode& node, const ImVec2& nodePos, const ImVec2& nodeSize)
{
    if (m_inlineNameNodeId == node.id) {
        ImGui::SetCursorScreenPos(ImVec2(nodePos.x + 10.0f * m_graphZoom, nodePos.y + 7.0f * m_graphZoom));
        ImGui::SetNextItemWidth(nodeSize.x - 58.0f * m_graphZoom);
        if (m_focusInlineEditor) {
            ImGui::SetKeyboardFocusHere();
            m_focusInlineEditor = false;
        }
        if (ImGui::InputText(("##name" + std::to_string(node.id)).c_str(), m_inlineNameBuf, sizeof(m_inlineNameBuf),
            ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll)) {
            node.name = m_inlineNameBuf;
            m_inlineNameNodeId = 0;
            m_dirty = true;
        }
    }
    if (m_inlinePathNodeId == node.id) {
        ImGui::SetCursorScreenPos(ImVec2(nodePos.x + 10.0f * m_graphZoom, nodePos.y + 32.0f * m_graphZoom));
        ImGui::SetNextItemWidth(nodeSize.x - 20.0f * m_graphZoom);
        if (m_focusInlineEditor) {
            ImGui::SetKeyboardFocusHere();
            m_focusInlineEditor = false;
        }
        if (ImGui::InputText(("##path" + std::to_string(node.id)).c_str(), m_inlinePathBuf, sizeof(m_inlinePathBuf),
            ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll)) {
            const std::string normalized = NormalizeGameLoopScenePath(m_inlinePathBuf);
            node.scenePath = normalized.empty() ? std::string(m_inlinePathBuf) : normalized;
            m_inlinePathNodeId = 0;
            m_dirty = true;
        }
    }
}

void GameLoopEditorPanel::OpenFloatingTransitionEditor(uint32_t transitionId, const ImVec2& screenPos)
{
    if (transitionId == 0) return;
    SelectTransition(transitionId);
    m_floatingTransitionId = transitionId;
    m_floatingEditorScreenPos = { screenPos.x, screenPos.y };
    m_floatingTransitionEditorOpen = true;
}

std::string GameLoopEditorPanel::BuildConditionSummary(const GameLoopCondition& condition) const
{
    char buf[256];
    switch (condition.type) {
    case GameLoopConditionType::InputPressed:
        std::snprintf(buf, sizeof(buf), "Input %s", ActionLabel(condition.actionIndex));
        return buf;
    case GameLoopConditionType::UIButtonClicked:
        std::snprintf(buf, sizeof(buf), "Button %s", condition.targetName.empty() ? "(empty)" : condition.targetName.c_str());
        return buf;
    case GameLoopConditionType::TimerElapsed:
        std::snprintf(buf, sizeof(buf), "Timer %.1fs", condition.seconds);
        return buf;
    case GameLoopConditionType::ActorDead:
        std::snprintf(buf, sizeof(buf), "%s Dead", ActorLabel(condition.actorType));
        return buf;
    case GameLoopConditionType::AllActorsDead:
        std::snprintf(buf, sizeof(buf), "All %s Dead", ActorLabel(condition.actorType));
        return buf;
    case GameLoopConditionType::ActorMovedDistance:
        std::snprintf(buf, sizeof(buf), "%s Moved %.1f", ActorLabel(condition.actorType), condition.threshold);
        return buf;
    case GameLoopConditionType::RuntimeFlag:
        std::snprintf(buf, sizeof(buf), "Flag %s", condition.parameterName.empty() ? "(empty)" : condition.parameterName.c_str());
        return buf;
    case GameLoopConditionType::StateMachineState:
        std::snprintf(buf, sizeof(buf), "State %s", condition.parameterName.empty() ? "(empty)" : condition.parameterName.c_str());
        return buf;
    case GameLoopConditionType::TimelineEvent:
    case GameLoopConditionType::CustomEvent:
        std::snprintf(buf, sizeof(buf), "Event %s", condition.eventName.empty() ? "(empty)" : condition.eventName.c_str());
        return buf;
    default:
        return "None";
    }
}

std::string GameLoopEditorPanel::BuildTransitionLabel(const GameLoopTransition& transition, bool compact) const
{
    std::string label = "#" + std::to_string(GetSourceLocalPriority(transition)) + " ";
    if (transition.conditions.empty()) {
        label += "No Condition";
    } else if (compact) {
        label += BuildConditionSummary(transition.conditions.front());
        if (transition.conditions.size() > 1) {
            label += " C:" + std::to_string(transition.conditions.size());
        }
    } else {
        label += BuildConditionSummary(transition.conditions.front());
        if (transition.conditions.size() > 1) {
            label += " +" + std::to_string(transition.conditions.size() - 1);
        }
    }
    if (transition.loadingPolicy.mode != GameLoopLoadingMode::Immediate) {
        label += " [";
        label += LoadingLabel(transition.loadingPolicy.mode);
        label += "]";
    }
    return label;
}

std::string GameLoopEditorPanel::BuildMiddleEllipsis(const std::string& text, float maxWidth) const
{
    if (text.empty() || ImGui::CalcTextSize(text.c_str()).x <= maxWidth) return text;
    const char* ellipsis = "...";
    if (ImGui::CalcTextSize(ellipsis).x > maxWidth) return ellipsis;

    size_t left = text.size() / 2;
    size_t right = text.size() - left;
    std::string result = text.substr(0, left) + ellipsis + text.substr(text.size() - right);
    while (left > 2 && right > 2 && ImGui::CalcTextSize(result.c_str()).x > maxWidth) {
        if (left > right) --left;
        else --right;
        result = text.substr(0, left) + ellipsis + text.substr(text.size() - right);
    }
    return result;
}

void GameLoopEditorPanel::DoValidate()
{
    m_lastValidate = ValidateGameLoopAsset(Asset());
    m_validatedOnce = true;
}

void GameLoopEditorPanel::DoSave()
{
    DoValidate();
    if (m_lastValidate.HasError()) {
        LOG_WARN("[GameLoop] Validate has errors. Save aborted.");
        return;
    }
    if (Asset().SaveToFile(m_currentPath)) {
        LOG_INFO("[GameLoop] saved to %s", m_currentPath.string().c_str());
        m_dirty = false;
    } else {
        LOG_ERROR("[GameLoop] save failed: %s", m_currentPath.string().c_str());
    }
}

void GameLoopEditorPanel::DoSaveAs(const std::filesystem::path& newPath)
{
    if (newPath.empty()) return;
    m_currentPath = newPath;
    DoSave();
}

void GameLoopEditorPanel::DoLoad(const std::filesystem::path& fromPath)
{
    if (fromPath.empty()) return;
    if (Asset().LoadFromFile(fromPath)) {
        m_currentPath = fromPath;
        ClearSelection();
        m_validatedOnce = false;
        m_dirty = false;
        RequestFitGraph();
        LOG_INFO("[GameLoop] loaded %s", fromPath.string().c_str());
    } else {
        LOG_ERROR("[GameLoop] load failed: %s", fromPath.string().c_str());
    }
}

void GameLoopEditorPanel::DoNewDefault()
{
    Asset() = GameLoopAsset::CreateDefault();
    ClearSelection();
    RequestFitGraph();
    m_dirty = true;
}

void GameLoopEditorPanel::ApplyZTestLoopPreset()
{
    Asset() = GameLoopAsset::CreateZTestLoop();
    ClearSelection();
    RequestFitGraph();
    m_dirty = true;
}
