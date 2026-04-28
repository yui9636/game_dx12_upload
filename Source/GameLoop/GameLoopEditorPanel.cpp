#include "GameLoopEditorPanel.h"

#include <algorithm>
#include <cstdio>
#include <cstring>

#include <imgui.h>

#include "Console/Logger.h"
#include "Engine/EngineKernel.h"
#include "GameLoopAsset.h"
#include "GameLoopRuntime.h"

namespace
{
    constexpr float kNodeWidth = 230.0f;
    constexpr float kNodeHeight = 82.0f;
    constexpr float kPinRadius = 6.0f;

    // Returns the global GameLoop asset owned by EngineKernel.
    GameLoopAsset& Asset() { return EngineKernel::Instance().GetGameLoopAsset(); }

    // Returns the global GameLoop runtime owned by EngineKernel.
    const GameLoopRuntime& Runtime() { return EngineKernel::Instance().GetGameLoopRuntime(); }

    const char* kConditionTypeLabels[] = {
        "None", "InputPressed", "UIButtonClicked", "TimerElapsed",
        "ActorDead", "AllActorsDead", "ActorMovedDistance", "RuntimeFlag",
        "StateMachineState", "TimelineEvent", "CustomEvent"
    };
    const GameLoopConditionType kConditionTypeValues[] = {
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
    constexpr int kConditionTypeCount = sizeof(kConditionTypeValues) / sizeof(kConditionTypeValues[0]);

    const char* kActorTypeLabels[] = { "None", "Player", "Enemy", "NPC", "Neutral" };
    const ActorType kActorTypeValues[] = {
        ActorType::None, ActorType::Player, ActorType::Enemy, ActorType::NPC, ActorType::Neutral
    };
    constexpr int kActorTypeCount = sizeof(kActorTypeValues) / sizeof(kActorTypeValues[0]);

    const char* kActionLabels[] = { "Confirm", "Cancel", "Retry" };
    constexpr int kActionCount = sizeof(kActionLabels) / sizeof(kActionLabels[0]);

    // Finds combo index from a condition enum value.
    int FindConditionTypeIndex(GameLoopConditionType t)
    {
        for (int i = 0; i < kConditionTypeCount; ++i) {
            if (kConditionTypeValues[i] == t) return i;
        }
        return 0;
    }

    // Finds combo index from an actor enum value.
    int FindActorTypeIndex(ActorType a)
    {
        for (int i = 0; i < kActorTypeCount; ++i) {
            if (kActorTypeValues[i] == a) return i;
        }
        return 0;
    }

    // Returns action label for GameLoop input action index.
    const char* ActionIndexToLabel(int actionIndex)
    {
        if (actionIndex >= 0 && actionIndex < kActionCount) {
            return kActionLabels[actionIndex];
        }
        return "Unknown";
    }

    // Returns actor label for summary text.
    const char* ActorTypeToLabel(ActorType actorType)
    {
        switch (actorType) {
        case ActorType::Player:  return "Player";
        case ActorType::Enemy:   return "Enemy";
        case ActorType::NPC:     return "NPC";
        case ActorType::Neutral: return "Neutral";
        default:                 return "None";
        }
    }

    // Adds two ImVec2 values.
    ImVec2 AddVec2(const ImVec2& a, const ImVec2& b)
    {
        return ImVec2(a.x + b.x, a.y + b.y);
    }

    // Subtracts two ImVec2 values.
    ImVec2 SubVec2(const ImVec2& a, const ImVec2& b)
    {
        return ImVec2(a.x - b.x, a.y - b.y);
    }
}

GameLoopEditorPanel::GameLoopEditorPanel()
{
    std::strncpy(m_loadPathBuf, m_currentPath.string().c_str(), sizeof(m_loadPathBuf) - 1);
    std::strncpy(m_saveAsPathBuf, m_currentPath.string().c_str(), sizeof(m_saveAsPathBuf) - 1);
}

void GameLoopEditorPanel::Draw(bool* p_open, bool* outFocused)
{
    if (!ImGui::Begin("GameLoop Editor", p_open)) {
        if (outFocused) *outFocused = false;
        ImGui::End();
        return;
    }
    if (outFocused) *outFocused = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);

    DrawToolbar();
    ImGui::Separator();

    const float outlinerWidth = 260.0f;
    const float inspectorHeight = 260.0f;
    ImVec2 available = ImGui::GetContentRegionAvail();
    float upperHeight = available.y - inspectorHeight - ImGui::GetStyle().ItemSpacing.y;
    if (upperHeight < 260.0f) {
        upperHeight = 260.0f;
    }

    ImGui::BeginChild("GameLoopUpper", ImVec2(0.0f, upperHeight), false);
    ImGui::BeginChild("GameLoopOutliner", ImVec2(outlinerWidth, 0.0f), true);
    DrawOutliner();
    ImGui::EndChild();

    ImGui::SameLine();

    ImGui::BeginChild("GameLoopGraphHost", ImVec2(0.0f, 0.0f), true);
    DrawGraphCanvas(ImGui::GetContentRegionAvail());
    ImGui::EndChild();
    ImGui::EndChild();

    ImGui::BeginChild("GameLoopInspector", ImVec2(0.0f, 0.0f), true);
    DrawInspector();
    ImGui::EndChild();

    ImGui::End();
}

void GameLoopEditorPanel::DrawToolbar()
{
    if (ImGui::Button("New Default Loop")) {
        DoNewDefault();
    }
    ImGui::SameLine();
    if (ImGui::Button("Z Test Loop")) {
        ApplyZTestLoopPreset();
    }
    ImGui::SameLine();
    if (ImGui::Button("Validate")) {
        DoValidate();
    }
    ImGui::SameLine();
    if (ImGui::Button("Save")) {
        DoSave();
    }
    ImGui::SameLine();
    if (ImGui::Button("Load...")) {
        std::strncpy(m_loadPathBuf, m_currentPath.string().c_str(), sizeof(m_loadPathBuf) - 1);
        ImGui::OpenPopup("Load GameLoop");
    }
    ImGui::SameLine();
    if (ImGui::Button("Save As...")) {
        std::strncpy(m_saveAsPathBuf, m_currentPath.string().c_str(), sizeof(m_saveAsPathBuf) - 1);
        ImGui::OpenPopup("Save GameLoop As");
    }

    ImGui::SameLine();
    ImGui::TextDisabled("Current: %s", m_currentPath.string().c_str());

    if (ImGui::BeginPopupModal("Load GameLoop", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::InputText("Path", m_loadPathBuf, sizeof(m_loadPathBuf));
        if (ImGui::Button("Load")) {
            DoLoad(std::filesystem::path(m_loadPathBuf));
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel")) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    if (ImGui::BeginPopupModal("Save GameLoop As", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::InputText("Path", m_saveAsPathBuf, sizeof(m_saveAsPathBuf));
        if (ImGui::Button("Save As")) {
            DoSaveAs(std::filesystem::path(m_saveAsPathBuf));
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel")) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
}

void GameLoopEditorPanel::DrawOutliner()
{
    GameLoopAsset& asset = Asset();

    if (ImGui::CollapsingHeader("Nodes", ImGuiTreeNodeFlags_DefaultOpen)) {
        if (ImGui::Button("Add Node")) {
            GameLoopNode node;
            node.id = asset.AllocateNodeId();
            node.name = "NewNode";
            node.scenePath = "Data/Scenes/New.scene";
            node.graphPos = { 120.0f, 120.0f };
            asset.nodes.push_back(node);
            m_selectedNodeId = node.id;
            m_selectedTransitionIndex = -1;
            m_selectedConditionIndex = -1;
        }

        for (const GameLoopNode& node : asset.nodes) {
            char label[256];
            std::snprintf(label, sizeof(label), "%s%s [id=%u]",
                node.id == asset.startNodeId ? "* " : "  ",
                node.name.c_str(),
                node.id);
            if (ImGui::Selectable(label, m_selectedNodeId == node.id)) {
                m_selectedNodeId = node.id;
                m_selectedTransitionIndex = -1;
                m_selectedConditionIndex = -1;
            }
        }
    }

    if (ImGui::CollapsingHeader("Transitions", ImGuiTreeNodeFlags_DefaultOpen)) {
        if (ImGui::Button("Add Transition")) {
            if (asset.nodes.size() >= 2) {
                AddTransition(asset.nodes[0].id, asset.nodes[1].id);
            }
        }

        for (int i = 0; i < static_cast<int>(asset.transitions.size()); ++i) {
            const GameLoopTransition& transition = asset.transitions[i];
            const GameLoopNode* fromNode = asset.FindNode(transition.fromNodeId);
            const GameLoopNode* toNode = asset.FindNode(transition.toNodeId);
            char label[320];
            std::snprintf(label, sizeof(label), "%s -> %s : %s",
                fromNode ? fromNode->name.c_str() : "(?)",
                toNode ? toNode->name.c_str() : "(?)",
                BuildTransitionSummary(transition).c_str());
            if (ImGui::Selectable(label, m_selectedTransitionIndex == i)) {
                m_selectedTransitionIndex = i;
                m_selectedNodeId = 0;
                m_selectedConditionIndex = -1;
            }
        }
    }

    ImGui::Separator();
    DrawRuntimeStatus();
    ImGui::Separator();
    DrawValidateResult();
}

void GameLoopEditorPanel::DrawGraphCanvas(const ImVec2& size)
{
    GameLoopAsset& asset = Asset();
    ImDrawList* drawList = ImGui::GetWindowDrawList();

    ImVec2 canvasOrigin = ImGui::GetCursorScreenPos();
    ImVec2 canvasSize = size;
    if (canvasSize.x < 100.0f) canvasSize.x = 100.0f;
    if (canvasSize.y < 100.0f) canvasSize.y = 100.0f;
    ImVec2 canvasEnd = AddVec2(canvasOrigin, canvasSize);

    drawList->AddRectFilled(canvasOrigin, canvasEnd, IM_COL32(30, 32, 36, 255));
    drawList->AddRect(canvasOrigin, canvasEnd, IM_COL32(70, 70, 80, 255));

    ImGui::SetCursorScreenPos(canvasOrigin);
    ImGui::InvisibleButton("GameLoopGraphCanvasBg", canvasSize);
    const bool canvasHovered = ImGui::IsItemHovered();

    if (canvasHovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !m_isConnecting) {
        m_selectedNodeId = 0;
        m_selectedTransitionIndex = -1;
        m_selectedConditionIndex = -1;
    }

    if (canvasHovered && ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
        m_contextGraphPosition = ScreenToGraph(ImGui::GetIO().MousePos, canvasOrigin);
        ImGui::OpenPopup("GameLoopGraphContext");
    }

    if (canvasHovered && ImGui::IsMouseDragging(ImGuiMouseButton_Right, 0.0f)) {
        ImVec2 delta = ImGui::GetIO().MouseDelta;
        m_graphOffset.x += delta.x;
        m_graphOffset.y += delta.y;
    }

    if (canvasHovered && ImGui::GetIO().MouseWheel != 0.0f) {
        m_graphZoom += ImGui::GetIO().MouseWheel * 0.08f;
        if (m_graphZoom < 0.35f) m_graphZoom = 0.35f;
        if (m_graphZoom > 2.25f) m_graphZoom = 2.25f;
    }

    if (m_isConnecting && ImGui::IsKeyPressed(ImGuiKey_Escape)) {
        m_isConnecting = false;
        m_connectFromNodeId = 0;
    }

    for (int i = 0; i < static_cast<int>(asset.transitions.size()); ++i) {
        DrawGraphTransition(i, canvasOrigin);
    }

    for (GameLoopNode& node : asset.nodes) {
        DrawGraphNode(node, canvasOrigin);
    }

    if (m_isConnecting) {
        GameLoopNode* fromNode = asset.FindNode(m_connectFromNodeId);
        if (fromNode) {
            ImVec2 fromPos = GraphToScreen(fromNode->graphPos, canvasOrigin);
            ImVec2 start = ImVec2(fromPos.x + kNodeWidth * m_graphZoom, fromPos.y + (kNodeHeight * 0.5f) * m_graphZoom);
            ImVec2 mouse = ImGui::GetIO().MousePos;
            drawList->AddBezierCubic(
                start,
                ImVec2(start.x + 90.0f * m_graphZoom, start.y),
                ImVec2(mouse.x - 90.0f * m_graphZoom, mouse.y),
                mouse,
                IM_COL32(255, 220, 90, 255),
                2.0f);
        }
    }

    DrawGraphContextMenu(ImVec2(m_contextGraphPosition.x, m_contextGraphPosition.y));
}

void GameLoopEditorPanel::DrawGraphNode(GameLoopNode& node, const ImVec2& canvasOrigin)
{
    GameLoopAsset& asset = Asset();
    const GameLoopRuntime& runtime = Runtime();
    ImDrawList* drawList = ImGui::GetWindowDrawList();

    ImVec2 nodePos = GraphToScreen(node.graphPos, canvasOrigin);
    ImVec2 nodeSize(kNodeWidth * m_graphZoom, kNodeHeight * m_graphZoom);
    ImVec2 nodeEnd = AddVec2(nodePos, nodeSize);

    bool selected = (m_selectedNodeId == node.id);
    bool isStart = (asset.startNodeId == node.id);
    bool isCurrent = (runtime.isActive && runtime.currentNodeId == node.id);
    bool isPending = (runtime.sceneTransitionRequested && runtime.pendingNodeId == node.id);

    ImU32 fillColor = IM_COL32(48, 52, 62, 255);
    ImU32 borderColor = IM_COL32(110, 110, 125, 255);
    if (isStart) borderColor = IM_COL32(90, 150, 255, 255);
    if (isCurrent) borderColor = IM_COL32(80, 220, 120, 255);
    if (isPending) borderColor = IM_COL32(255, 210, 80, 255);
    if (selected) fillColor = IM_COL32(62, 70, 92, 255);

    drawList->AddRectFilled(nodePos, nodeEnd, fillColor, 8.0f * m_graphZoom);
    drawList->AddRect(nodePos, nodeEnd, borderColor, 8.0f * m_graphZoom, 0, selected ? 3.0f : 2.0f);

    ImVec2 inputPin(nodePos.x, nodePos.y + nodeSize.y * 0.5f);
    ImVec2 outputPin(nodeEnd.x, nodePos.y + nodeSize.y * 0.5f);
    drawList->AddCircleFilled(inputPin, kPinRadius * m_graphZoom, IM_COL32(130, 170, 230, 255));
    drawList->AddCircleFilled(outputPin, kPinRadius * m_graphZoom, IM_COL32(230, 170, 90, 255));

    drawList->AddText(ImVec2(nodePos.x + 12.0f * m_graphZoom, nodePos.y + 10.0f * m_graphZoom), IM_COL32(255, 255, 255, 255), node.name.c_str());
    drawList->AddText(ImVec2(nodePos.x + 12.0f * m_graphZoom, nodePos.y + 34.0f * m_graphZoom), IM_COL32(180, 185, 195, 255), node.scenePath.c_str());
    if (isStart) {
        drawList->AddText(ImVec2(nodePos.x + 12.0f * m_graphZoom, nodePos.y + 58.0f * m_graphZoom), IM_COL32(125, 175, 255, 255), "START");
    }
    if (isCurrent) {
        drawList->AddText(ImVec2(nodeEnd.x - 80.0f * m_graphZoom, nodePos.y + 58.0f * m_graphZoom), IM_COL32(110, 255, 150, 255), "CURRENT");
    }

    ImGui::SetCursorScreenPos(nodePos);
    ImGui::InvisibleButton(("node_body##" + std::to_string(node.id)).c_str(), nodeSize);
    if (ImGui::IsItemClicked(ImGuiMouseButton_Left)) {
        m_selectedNodeId = node.id;
        m_selectedTransitionIndex = -1;
        m_selectedConditionIndex = -1;
    }
    if (ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Left, 0.0f)) {
        ImVec2 delta = ImGui::GetIO().MouseDelta;
        node.graphPos.x += delta.x / m_graphZoom;
        node.graphPos.y += delta.y / m_graphZoom;
    }

    ImVec2 outputHitPos(outputPin.x - 8.0f * m_graphZoom, outputPin.y - 8.0f * m_graphZoom);
    ImGui::SetCursorScreenPos(outputHitPos);
    ImGui::InvisibleButton(("output_pin##" + std::to_string(node.id)).c_str(), ImVec2(16.0f * m_graphZoom, 16.0f * m_graphZoom));
    if (ImGui::IsItemClicked(ImGuiMouseButton_Left)) {
        m_isConnecting = true;
        m_connectFromNodeId = node.id;
    }

    ImVec2 inputHitPos(inputPin.x - 8.0f * m_graphZoom, inputPin.y - 8.0f * m_graphZoom);
    ImGui::SetCursorScreenPos(inputHitPos);
    ImGui::InvisibleButton(("input_pin##" + std::to_string(node.id)).c_str(), ImVec2(16.0f * m_graphZoom, 16.0f * m_graphZoom));
    if (ImGui::IsItemClicked(ImGuiMouseButton_Left) && m_isConnecting) {
        if (m_connectFromNodeId != 0 && m_connectFromNodeId != node.id) {
            AddTransition(m_connectFromNodeId, node.id);
        }
        m_isConnecting = false;
        m_connectFromNodeId = 0;
    }
}

void GameLoopEditorPanel::DrawGraphTransition(int transitionIndex, const ImVec2& canvasOrigin)
{
    GameLoopAsset& asset = Asset();
    if (transitionIndex < 0 || transitionIndex >= static_cast<int>(asset.transitions.size())) {
        return;
    }

    const GameLoopTransition& transition = asset.transitions[transitionIndex];
    const GameLoopNode* fromNode = asset.FindNode(transition.fromNodeId);
    const GameLoopNode* toNode = asset.FindNode(transition.toNodeId);
    if (!fromNode || !toNode) {
        return;
    }

    ImDrawList* drawList = ImGui::GetWindowDrawList();
    ImVec2 fromPos = GraphToScreen(fromNode->graphPos, canvasOrigin);
    ImVec2 toPos = GraphToScreen(toNode->graphPos, canvasOrigin);

    ImVec2 start(fromPos.x + kNodeWidth * m_graphZoom, fromPos.y + (kNodeHeight * 0.5f) * m_graphZoom);
    ImVec2 end(toPos.x, toPos.y + (kNodeHeight * 0.5f) * m_graphZoom);
    ImVec2 c1(start.x + 90.0f * m_graphZoom, start.y);
    ImVec2 c2(end.x - 90.0f * m_graphZoom, end.y);

    bool selected = (m_selectedTransitionIndex == transitionIndex);
    ImU32 color = selected ? IM_COL32(255, 225, 90, 255) : IM_COL32(150, 160, 180, 255);
    drawList->AddBezierCubic(start, c1, c2, end, color, selected ? 3.0f : 2.0f);
    drawList->AddTriangleFilled(
        ImVec2(end.x - 10.0f * m_graphZoom, end.y - 5.0f * m_graphZoom),
        ImVec2(end.x - 10.0f * m_graphZoom, end.y + 5.0f * m_graphZoom),
        ImVec2(end.x, end.y),
        color);

    ImVec2 mid((start.x + end.x) * 0.5f, (start.y + end.y) * 0.5f);
    std::string summary = BuildTransitionSummary(transition);
    ImVec2 textSize = ImGui::CalcTextSize(summary.c_str());
    ImVec2 labelMin(mid.x - textSize.x * 0.5f - 6.0f, mid.y - textSize.y * 0.5f - 4.0f);
    ImVec2 labelMax(mid.x + textSize.x * 0.5f + 6.0f, mid.y + textSize.y * 0.5f + 4.0f);
    drawList->AddRectFilled(labelMin, labelMax, IM_COL32(28, 30, 35, 230), 4.0f);
    drawList->AddRect(labelMin, labelMax, color, 4.0f);
    drawList->AddText(ImVec2(labelMin.x + 6.0f, labelMin.y + 4.0f), IM_COL32(235, 235, 235, 255), summary.c_str());

    ImGui::SetCursorScreenPos(labelMin);
    ImGui::InvisibleButton(("edge_hit##" + std::to_string(transitionIndex)).c_str(), SubVec2(labelMax, labelMin));
    if (ImGui::IsItemClicked(ImGuiMouseButton_Left)) {
        m_selectedTransitionIndex = transitionIndex;
        m_selectedNodeId = 0;
        m_selectedConditionIndex = -1;
    }
}

void GameLoopEditorPanel::DrawGraphContextMenu(const ImVec2& graphMousePosition)
{
    if (ImGui::BeginPopup("GameLoopGraphContext")) {
        if (ImGui::MenuItem("Add Scene Node")) {
            GameLoopAsset& asset = Asset();
            GameLoopNode node;
            node.id = asset.AllocateNodeId();
            node.name = "NewNode";
            node.scenePath = "Data/Scenes/New.scene";
            node.graphPos = { graphMousePosition.x, graphMousePosition.y };
            asset.nodes.push_back(node);
            m_selectedNodeId = node.id;
            m_selectedTransitionIndex = -1;
            m_selectedConditionIndex = -1;
        }
        if (ImGui::MenuItem("Apply Z Test Loop")) {
            ApplyZTestLoopPreset();
        }
        ImGui::EndPopup();
    }
}

void GameLoopEditorPanel::DrawInspector()
{
    GameLoopNode* selectedNode = FindSelectedNode();
    GameLoopTransition* selectedTransition = FindSelectedTransition();

    if (selectedNode) {
        DrawNodeInspector(*selectedNode);
        return;
    }

    if (selectedTransition) {
        DrawTransitionInspector(*selectedTransition);
        return;
    }

    ImGui::TextDisabled("Select a scene node or transition edge to edit details.");
}

void GameLoopEditorPanel::DrawNodeInspector(GameLoopNode& node)
{
    GameLoopAsset& asset = Asset();
    ImGui::TextUnformatted("Node Inspector");
    ImGui::Separator();

    char nameBuf[128] = {};
    std::strncpy(nameBuf, node.name.c_str(), sizeof(nameBuf) - 1);
    if (ImGui::InputText("Name", nameBuf, sizeof(nameBuf))) {
        node.name = nameBuf;
    }

    char pathBuf[256] = {};
    std::strncpy(pathBuf, node.scenePath.c_str(), sizeof(pathBuf) - 1);
    if (ImGui::InputText("Scene Path", pathBuf, sizeof(pathBuf))) {
        node.scenePath = pathBuf;
    }

    ImGui::Text("Id: %u", node.id);
    ImGui::InputFloat("Graph X", &node.graphPos.x);
    ImGui::InputFloat("Graph Y", &node.graphPos.y);

    if (node.id != asset.startNodeId) {
        if (ImGui::Button("Set As Start")) {
            asset.startNodeId = node.id;
        }
    }
    else {
        ImGui::TextDisabled("[ Start Node ]");
    }

    ImGui::SameLine();
    if (ImGui::Button("Delete Node")) {
        const uint32_t removedId = node.id;
        asset.nodes.erase(
            std::remove_if(asset.nodes.begin(), asset.nodes.end(),
                [removedId](const GameLoopNode& n) { return n.id == removedId; }),
            asset.nodes.end());
        asset.transitions.erase(
            std::remove_if(asset.transitions.begin(), asset.transitions.end(),
                [removedId](const GameLoopTransition& t) {
                    return t.fromNodeId == removedId || t.toNodeId == removedId;
                }),
            asset.transitions.end());
        if (asset.startNodeId == removedId) {
            asset.startNodeId = asset.nodes.empty() ? 0u : asset.nodes.front().id;
        }
        m_selectedNodeId = 0;
        m_selectedTransitionIndex = -1;
        m_selectedConditionIndex = -1;
    }
}

void GameLoopEditorPanel::DrawTransitionInspector(GameLoopTransition& transition)
{
    GameLoopAsset& asset = Asset();
    ImGui::TextUnformatted("Transition Inspector");
    ImGui::Separator();

    char nameBuf[128] = {};
    std::strncpy(nameBuf, transition.name.c_str(), sizeof(nameBuf) - 1);
    if (ImGui::InputText("Name", nameBuf, sizeof(nameBuf))) {
        transition.name = nameBuf;
    }

    auto DrawNodeCombo = [&asset](const char* label, uint32_t& value) {
        const GameLoopNode* selected = asset.FindNode(value);
        const char* preview = selected ? selected->name.c_str() : "(none)";
        if (ImGui::BeginCombo(label, preview)) {
            for (const GameLoopNode& node : asset.nodes) {
                const bool isSelected = (node.id == value);
                if (ImGui::Selectable(node.name.c_str(), isSelected)) {
                    value = node.id;
                }
                if (isSelected) {
                    ImGui::SetItemDefaultFocus();
                }
            }
            ImGui::EndCombo();
        }
        };

    DrawNodeCombo("From", transition.fromNodeId);
    DrawNodeCombo("To", transition.toNodeId);

    int modeIndex = transition.requireAllConditions ? 1 : 0;
    const char* modeLabels[] = { "Any", "All" };
    if (ImGui::Combo("Condition Mode", &modeIndex, modeLabels, 2)) {
        transition.requireAllConditions = (modeIndex == 1);
    }

    ImGui::Spacing();
    DrawConditionList(transition);
}

void GameLoopEditorPanel::DrawConditionList(GameLoopTransition& transition)
{
    ImGui::TextUnformatted("Conditions");
    if (ImGui::Button("Add Condition")) {
        GameLoopCondition condition;
        condition.type = GameLoopConditionType::InputPressed;
        condition.actionIndex = 0;
        transition.conditions.push_back(condition);
        m_selectedConditionIndex = static_cast<int>(transition.conditions.size()) - 1;
    }

    int removeAt = -1;
    ImGui::BeginChild("GameLoopConditionList", ImVec2(360.0f, 120.0f), true);
    for (int i = 0; i < static_cast<int>(transition.conditions.size()); ++i) {
        ImGui::PushID(i);
        std::string summary = BuildConditionSummary(transition.conditions[i]);
        if (ImGui::Selectable(summary.c_str(), m_selectedConditionIndex == i)) {
            m_selectedConditionIndex = i;
        }
        ImGui::SameLine();
        if (ImGui::SmallButton("X")) {
            removeAt = i;
        }
        ImGui::PopID();
    }
    ImGui::EndChild();

    if (removeAt >= 0) {
        transition.conditions.erase(transition.conditions.begin() + removeAt);
        if (m_selectedConditionIndex == removeAt) {
            m_selectedConditionIndex = -1;
        }
        if (m_selectedConditionIndex > removeAt) {
            --m_selectedConditionIndex;
        }
    }

    if (m_selectedConditionIndex >= 0 && m_selectedConditionIndex < static_cast<int>(transition.conditions.size())) {
        ImGui::Spacing();
        DrawSelectedConditionInspector(transition.conditions[m_selectedConditionIndex]);
    }
    else {
        ImGui::TextDisabled("Select a condition to edit its details.");
    }
}

void GameLoopEditorPanel::DrawSelectedConditionInspector(GameLoopCondition& condition)
{
    ImGui::Separator();
    ImGui::TextUnformatted("Selected Condition");

    int typeIndex = FindConditionTypeIndex(condition.type);
    if (ImGui::Combo("Type", &typeIndex, kConditionTypeLabels, kConditionTypeCount)) {
        condition.type = kConditionTypeValues[typeIndex];
    }

    switch (condition.type) {
    case GameLoopConditionType::None:
        ImGui::TextDisabled("Always true.");
        break;

    case GameLoopConditionType::InputPressed:
    {
        int actionIndex = condition.actionIndex;
        if (actionIndex < 0 || actionIndex >= kActionCount) actionIndex = 0;
        if (ImGui::Combo("Action", &actionIndex, kActionLabels, kActionCount)) {
            condition.actionIndex = actionIndex;
        }
        break;
    }

    case GameLoopConditionType::UIButtonClicked:
    {
        char buf[128] = {};
        std::strncpy(buf, condition.targetName.c_str(), sizeof(buf) - 1);
        if (ImGui::InputText("Button Id", buf, sizeof(buf))) condition.targetName = buf;
        break;
    }

    case GameLoopConditionType::TimerElapsed:
        ImGui::InputFloat("Seconds", &condition.seconds);
        break;

    case GameLoopConditionType::ActorDead:
    case GameLoopConditionType::AllActorsDead:
    {
        int actorIndex = FindActorTypeIndex(condition.actorType);
        if (ImGui::Combo("Actor Type", &actorIndex, kActorTypeLabels, kActorTypeCount)) {
            condition.actorType = kActorTypeValues[actorIndex];
        }
        break;
    }

    case GameLoopConditionType::ActorMovedDistance:
    {
        int actorIndex = FindActorTypeIndex(condition.actorType);
        if (ImGui::Combo("Actor Type", &actorIndex, kActorTypeLabels, kActorTypeCount)) {
            condition.actorType = kActorTypeValues[actorIndex];
        }
        ImGui::InputFloat("Distance", &condition.threshold);
        break;
    }

    case GameLoopConditionType::RuntimeFlag:
    case GameLoopConditionType::StateMachineState:
    {
        char buf[128] = {};
        std::strncpy(buf, condition.parameterName.c_str(), sizeof(buf) - 1);
        if (ImGui::InputText("Parameter", buf, sizeof(buf))) condition.parameterName = buf;
        break;
    }

    case GameLoopConditionType::TimelineEvent:
    case GameLoopConditionType::CustomEvent:
    {
        char buf[128] = {};
        std::strncpy(buf, condition.eventName.c_str(), sizeof(buf) - 1);
        if (ImGui::InputText("Event", buf, sizeof(buf))) condition.eventName = buf;
        break;
    }

    default:
        break;
    }
}

void GameLoopEditorPanel::DrawValidateResult()
{
    if (!m_validatedOnce) {
        ImGui::TextDisabled("Validate: not run");
        return;
    }

    ImGui::Text("Validate: %d errors, %d warnings", m_lastValidate.ErrorCount(), m_lastValidate.WarningCount());
    if (ImGui::TreeNode("Validate Details")) {
        for (const GameLoopValidateMessage& message : m_lastValidate.messages) {
            ImVec4 color = ImVec4(0.8f, 0.8f, 0.8f, 1.0f);
            const char* tag = "[I]";
            if (message.severity == GameLoopValidateSeverity::Warning) { color = ImVec4(1.0f, 0.85f, 0.4f, 1.0f); tag = "[W]"; }
            if (message.severity == GameLoopValidateSeverity::Error) { color = ImVec4(1.0f, 0.4f, 0.4f, 1.0f); tag = "[E]"; }
            ImGui::TextColored(color, "%s %s", tag, message.message.c_str());
        }
        ImGui::TreePop();
    }
}

void GameLoopEditorPanel::DrawRuntimeStatus()
{
    const GameLoopRuntime& runtime = Runtime();
    const GameLoopAsset& asset = Asset();
    const GameLoopNode* currentNode = asset.FindNode(runtime.currentNodeId);
    const GameLoopNode* pendingNode = asset.FindNode(runtime.pendingNodeId);
    ImGui::Text("Runtime");
    ImGui::Text("active=%s", runtime.isActive ? "true" : "false");
    ImGui::Text("current=%s", currentNode ? currentNode->name.c_str() : "(none)");
    ImGui::Text("pending=%s%s", pendingNode ? pendingNode->name.c_str() : "(none)", runtime.sceneTransitionRequested ? " [requested]" : "");
    ImGui::Text("timer=%.2f", runtime.nodeTimer);
}

std::string GameLoopEditorPanel::BuildConditionSummary(const GameLoopCondition& condition) const
{
    char buf[256];
    switch (condition.type) {
    case GameLoopConditionType::None:
        return "Always";
    case GameLoopConditionType::InputPressed:
        std::snprintf(buf, sizeof(buf), "Input: %s", ActionIndexToLabel(condition.actionIndex));
        return buf;
    case GameLoopConditionType::UIButtonClicked:
        std::snprintf(buf, sizeof(buf), "Button: %s", condition.targetName.empty() ? "(empty)" : condition.targetName.c_str());
        return buf;
    case GameLoopConditionType::TimerElapsed:
        std::snprintf(buf, sizeof(buf), "Timer: %.2fs", condition.seconds);
        return buf;
    case GameLoopConditionType::ActorDead:
        std::snprintf(buf, sizeof(buf), "Dead: %s", ActorTypeToLabel(condition.actorType));
        return buf;
    case GameLoopConditionType::AllActorsDead:
        std::snprintf(buf, sizeof(buf), "AllDead: %s", ActorTypeToLabel(condition.actorType));
        return buf;
    case GameLoopConditionType::ActorMovedDistance:
        std::snprintf(buf, sizeof(buf), "Moved: %s %.2f", ActorTypeToLabel(condition.actorType), condition.threshold);
        return buf;
    case GameLoopConditionType::RuntimeFlag:
        std::snprintf(buf, sizeof(buf), "Flag: %s", condition.parameterName.empty() ? "(empty)" : condition.parameterName.c_str());
        return buf;
    case GameLoopConditionType::StateMachineState:
        std::snprintf(buf, sizeof(buf), "State: %s", condition.parameterName.empty() ? "(empty)" : condition.parameterName.c_str());
        return buf;
    case GameLoopConditionType::TimelineEvent:
        std::snprintf(buf, sizeof(buf), "Timeline: %s", condition.eventName.empty() ? "(empty)" : condition.eventName.c_str());
        return buf;
    case GameLoopConditionType::CustomEvent:
        std::snprintf(buf, sizeof(buf), "Event: %s", condition.eventName.empty() ? "(empty)" : condition.eventName.c_str());
        return buf;
    default:
        return "Unknown";
    }
}

std::string GameLoopEditorPanel::BuildTransitionSummary(const GameLoopTransition& transition) const
{
    if (transition.conditions.empty()) {
        return "(no condition)";
    }

    std::string result;
    const char* joinText = transition.requireAllConditions ? " AND " : " OR ";
    const int conditionCount = static_cast<int>(transition.conditions.size());
    for (int i = 0; i < conditionCount; ++i) {
        if (i > 0) result += joinText;
        if (i >= 3) {
            result += "...";
            break;
        }
        result += BuildConditionSummary(transition.conditions[i]);
    }
    return result;
}

GameLoopNode* GameLoopEditorPanel::FindSelectedNode()
{
    if (m_selectedNodeId == 0) {
        return nullptr;
    }
    return Asset().FindNode(m_selectedNodeId);
}

GameLoopTransition* GameLoopEditorPanel::FindSelectedTransition()
{
    GameLoopAsset& asset = Asset();
    if (m_selectedTransitionIndex < 0 || m_selectedTransitionIndex >= static_cast<int>(asset.transitions.size())) {
        return nullptr;
    }
    return &asset.transitions[m_selectedTransitionIndex];
}

ImVec2 GameLoopEditorPanel::GraphToScreen(const DirectX::XMFLOAT2& graphPos, const ImVec2& canvasOrigin) const
{
    return ImVec2(
        canvasOrigin.x + m_graphOffset.x + graphPos.x * m_graphZoom,
        canvasOrigin.y + m_graphOffset.y + graphPos.y * m_graphZoom);
}

DirectX::XMFLOAT2 GameLoopEditorPanel::ScreenToGraph(const ImVec2& screenPos, const ImVec2& canvasOrigin) const
{
    return DirectX::XMFLOAT2(
        (screenPos.x - canvasOrigin.x - m_graphOffset.x) / m_graphZoom,
        (screenPos.y - canvasOrigin.y - m_graphOffset.y) / m_graphZoom);
}

void GameLoopEditorPanel::AddTransition(uint32_t fromNodeId, uint32_t toNodeId)
{
    GameLoopAsset& asset = Asset();
    GameLoopTransition transition;
    transition.fromNodeId = fromNodeId;
    transition.toNodeId = toNodeId;
    transition.name = "NewTransition";
    transition.requireAllConditions = true;
    GameLoopCondition condition;
    condition.type = GameLoopConditionType::InputPressed;
    condition.actionIndex = 0;
    transition.conditions.push_back(condition);
    asset.transitions.push_back(transition);
    m_selectedTransitionIndex = static_cast<int>(asset.transitions.size()) - 1;
    m_selectedNodeId = 0;
    m_selectedConditionIndex = 0;
}

void GameLoopEditorPanel::ApplyZTestLoopPreset()
{
    Asset() = GameLoopAsset::CreateZTestLoop();
    m_selectedNodeId = 0;
    m_selectedTransitionIndex = -1;
    m_selectedConditionIndex = -1;
    m_validatedOnce = false;
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
    }
    else {
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
        m_selectedNodeId = 0;
        m_selectedTransitionIndex = -1;
        m_selectedConditionIndex = -1;
        m_validatedOnce = false;
        LOG_INFO("[GameLoop] loaded %s", fromPath.string().c_str());
    }
    else {
        LOG_ERROR("[GameLoop] load failed: %s", fromPath.string().c_str());
    }
}

void GameLoopEditorPanel::DoNewDefault()
{
    Asset() = GameLoopAsset::CreateDefault();
    m_selectedNodeId = 0;
    m_selectedTransitionIndex = -1;
    m_selectedConditionIndex = -1;
    m_validatedOnce = false;
}
