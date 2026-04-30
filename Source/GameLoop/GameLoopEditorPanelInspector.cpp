#include "GameLoopEditorPanelInternal.h"

namespace
{
    // ノード表示用のシーン名を scenePath から作る。
    // ノード名の手編集は廃止し、scenePath のファイル名 stem だけを正とする。
    std::string BuildInspectorSceneName(const GameLoopNode& node)
    {
        if (!node.scenePath.empty()) {
            return GameLoopScenePicker::BuildNodeNameFromScenePath(node.scenePath);
        }

        if (!node.name.empty()) {
            return node.name;
        }

        return "Scene";
    }

    // Inspector に scenePath を読み取り専用で表示する。
    // 直接入力によるパス編集は許可せず、Replace Scene / D&D だけで変更する。
    void DrawReadOnlyScenePath(const GameLoopNode& node)
    {
        if (node.scenePath.empty()) {
            ImGui::TextDisabled("Path: (no scene)");
            return;
        }

        ImGui::TextDisabled("Path: %s", node.scenePath.c_str());
    }
}

// 選択中のノード、Transition、または未選択状態の Inspector を描画する。
void GameLoopEditorPanelInternal::DrawInspector()
{
    if (m_selection == SelectionKind::Node) {
        GameLoopNode* node = FindNode(m_selectedNodeId);
        if (!node) {
            ClearSelection();
            return;
        }

        ImGui::TextUnformatted("Node");
        ImGui::Separator();

        const std::string sceneName = BuildInspectorSceneName(*node);
        ImGui::Text("Scene: %s", sceneName.c_str());
        DrawReadOnlyScenePath(*node);
        ImGui::Text("Id: %u", node->id);
        ImGui::Text("Start: %s", node->id == m_asset.startNodeId ? "Yes" : "No");

        std::string dropped;
        if (m_scenePicker.AcceptSceneAssetDragDrop(dropped)) {
            ReplaceNodeScene(node->id, dropped);
        }

        if (ImGui::Button("Replace Scene")) {
            OpenPickerForReplace(node->id);
        }
        ImGui::SameLine();
        if (ImGui::Button("Set Start")) {
            m_asset.startNodeId = node->id;
            m_dirty = true;
        }

        if (ImGui::Button("Delete")) {
            DeleteNode(node->id);
        }
        return;
    }

    if (m_selection == SelectionKind::Transition) {
        GameLoopTransition* transition = SelectedTransition();
        if (!transition) {
            ClearSelection();
            return;
        }

        const GameLoopNode* fromNode = FindNode(transition->fromNodeId);
        const GameLoopNode* toNode = FindNode(transition->toNodeId);
        const std::string fromName = fromNode ? BuildInspectorSceneName(*fromNode) : "?";
        const std::string toName = toNode ? BuildInspectorSceneName(*toNode) : "?";

        ImGui::TextUnformatted("Transition");
        ImGui::Separator();
        ImGui::Text("From: %s", fromName.c_str());
        ImGui::Text("To: %s", toName.c_str());

        int mode = transition->requireAllConditions ? 1 : 0;
        const char* modes[] = { "Any", "All" };
        if (ImGui::Combo("Mode", &mode, modes, 2)) {
            transition->requireAllConditions = mode == 1;
            m_dirty = true;
        }

        if (ImGui::Button("Add Condition")) {
            ImGui::OpenPopup(ConditionPopup);
        }
        ImGui::SameLine();
        if (ImGui::Button("Reverse")) {
            ReverseTransition(m_selectedTransitionIndex);
        }
        ImGui::SameLine();
        if (ImGui::Button("Delete")) {
            DeleteTransition(m_selectedTransitionIndex);
            return;
        }

        for (int i = 0; i < static_cast<int>(transition->conditions.size()); ++i) {
            const std::string label = ConditionLabel(transition->conditions[i]) + "##c" + std::to_string(i);
            if (ImGui::Selectable(label.c_str(), m_selectedConditionIndex == i)) {
                m_selectedConditionIndex = i;
            }
        }
        return;
    }

    ImGui::TextDisabled("No Selection");
}

// Validate の結果を下部パネルへ表示する。
void GameLoopEditorPanelInternal::DrawValidateSummary()
{
    if (!m_validated) {
        ImGui::TextDisabled("Validate has not been run.");
        return;
    }

    ImGui::Text("Errors: %d  Warnings: %d", m_validateResult.ErrorCount(), m_validateResult.WarningCount());

    if (ImGui::TreeNode("Details")) {
        for (const auto& message : m_validateResult.messages) {
            ImGui::TextWrapped("%s", message.message.c_str());
        }
        ImGui::TreePop();
    }
}

// Condition 追加用の簡易プリセット Popup を描画する。
void GameLoopEditorPanelInternal::DrawConditionPresetPopup()
{
    GameLoopTransition* transition = SelectedTransition();
    if (!transition) {
        return;
    }
    if (!ImGui::BeginPopup(ConditionPopup)) {
        return;
    }

    const char* labels[] = { "Confirm", "Retry", "Cancel", "StartButton", "Timer 3s", "All Enemy Dead", "Player Moved" };
    for (int i = 0; i < 7; ++i) {
        if (ImGui::MenuItem(labels[i])) {
            AddConditionPreset(*transition, i);
        }
    }

    ImGui::EndPopup();
}
