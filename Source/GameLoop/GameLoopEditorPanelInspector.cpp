#include "GameLoopEditorPanelInternal.h"

namespace
{
    struct ScancodeOption
    {
        uint32_t code;
        const char* label;
    };

    struct GamepadButtonOption
    {
        uint8_t code;
        const char* label;
    };

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

    void DrawReadOnlyScenePath(const GameLoopNode& node)
    {
        if (node.scenePath.empty()) {
            ImGui::TextDisabled("Path: (no scene)");
            return;
        }
        ImGui::TextDisabled("Path: %s", node.scenePath.c_str());
    }

    const ScancodeOption* GetScancodeOptions(int& outCount)
    {
        static const ScancodeOption options[] = {
            { 0, "Unbound" },
            { 4, "A" }, { 5, "B" }, { 6, "C" }, { 7, "D" }, { 8, "E" }, { 9, "F" },
            { 10, "G" }, { 11, "H" }, { 12, "I" }, { 13, "J" }, { 14, "K" }, { 15, "L" },
            { 16, "M" }, { 17, "N" }, { 18, "O" }, { 19, "P" }, { 20, "Q" }, { 21, "R" },
            { 22, "S" }, { 23, "T" }, { 24, "U" }, { 25, "V" }, { 26, "W" }, { 27, "X" },
            { 28, "Y" }, { 29, "Z" },
            { 30, "1" }, { 31, "2" }, { 32, "3" }, { 33, "4" }, { 34, "5" },
            { 35, "6" }, { 36, "7" }, { 37, "8" }, { 38, "9" }, { 39, "0" },
            { 40, "Enter" }, { 41, "Escape" }, { 42, "Backspace" }, { 43, "Tab" }, { 44, "Space" },
            { 79, "Right" }, { 80, "Left" }, { 81, "Down" }, { 82, "Up" },
            { 224, "LCtrl" }, { 225, "LShift" }, { 226, "LAlt" },
            { 228, "RCtrl" }, { 229, "RShift" }, { 230, "RAlt" },
        };
        outCount = sizeof(options) / sizeof(options[0]);
        return options;
    }

    const GamepadButtonOption* GetGamepadButtonOptions(int& outCount)
    {
        static const GamepadButtonOption options[] = {
            { 0xFF, "Unbound" },
            { 0, "A / Cross" },
            { 1, "B / Circle" },
            { 2, "X / Square" },
            { 3, "Y / Triangle" },
            { 4, "Back" },
            { 5, "Guide" },
            { 6, "Start" },
            { 7, "Left Stick" },
            { 8, "Right Stick" },
            { 9, "Left Shoulder" },
            { 10, "Right Shoulder" },
            { 11, "DPad Up" },
            { 12, "DPad Down" },
            { 13, "DPad Left" },
            { 14, "DPad Right" },
        };
        outCount = sizeof(options) / sizeof(options[0]);
        return options;
    }

    const char* GetScancodeLabel(uint32_t scancode)
    {
        int count = 0;
        const ScancodeOption* options = GetScancodeOptions(count);
        for (int i = 0; i < count; ++i) {
            if (options[i].code == scancode) return options[i].label;
        }
        return "Custom";
    }

    const char* GetGamepadButtonLabel(uint8_t button)
    {
        int count = 0;
        const GamepadButtonOption* options = GetGamepadButtonOptions(count);
        for (int i = 0; i < count; ++i) {
            if (options[i].code == button) return options[i].label;
        }
        return "Custom";
    }

    bool DrawKeyboardCombo(uint32_t& scancode)
    {
        bool changed = false;
        int count = 0;
        const ScancodeOption* options = GetScancodeOptions(count);
        if (ImGui::BeginCombo("Keyboard", GetScancodeLabel(scancode))) {
            for (int i = 0; i < count; ++i) {
                const bool selected = options[i].code == scancode;
                if (ImGui::Selectable(options[i].label, selected)) {
                    scancode = options[i].code;
                    changed = true;
                }
                if (selected) {
                    ImGui::SetItemDefaultFocus();
                }
            }
            ImGui::EndCombo();
        }
        return changed;
    }

    bool DrawGamepadCombo(uint8_t& button)
    {
        bool changed = false;
        int count = 0;
        const GamepadButtonOption* options = GetGamepadButtonOptions(count);
        if (ImGui::BeginCombo("Gamepad", GetGamepadButtonLabel(button))) {
            for (int i = 0; i < count; ++i) {
                const bool selected = options[i].code == button;
                if (ImGui::Selectable(options[i].label, selected)) {
                    button = options[i].code;
                    changed = true;
                }
                if (selected) {
                    ImGui::SetItemDefaultFocus();
                }
            }
            ImGui::EndCombo();
        }
        return changed;
    }

    bool DrawStringInput(const char* label, std::string& value)
    {
        char buffer[256] = {};
        std::strncpy(buffer, value.c_str(), sizeof(buffer) - 1);
        if (ImGui::InputText(label, buffer, sizeof(buffer))) {
            value = buffer;
            return true;
        }
        return false;
    }
}

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
        ImGui::Text("Id: %u", transition->id);

        ImGui::Spacing();
        ImGui::TextUnformatted("Input");
        ImGui::Separator();
        if (DrawKeyboardCombo(transition->input.keyboardScancode)) {
            m_dirty = true;
        }
        if (DrawGamepadCombo(transition->input.gamepadButton)) {
            m_dirty = true;
        }

        ImGui::Spacing();
        if (ImGui::Button("Reverse")) {
            ReverseTransition(m_selectedTransitionIndex);
        }
        ImGui::SameLine();
        if (ImGui::Button("Add Opposite")) {
            AddTransition(transition->toNodeId, transition->fromNodeId);
            return;
        }
        ImGui::SameLine();
        if (ImGui::Button("Delete")) {
            DeleteTransition(m_selectedTransitionIndex);
            return;
        }

        ImGui::Spacing();
        if (ImGui::CollapsingHeader("Loading Policy", ImGuiTreeNodeFlags_DefaultOpen)) {
            DrawLoadingPolicyEditor(*transition);
        }
        return;
    }

    ImGui::TextDisabled("No Selection");
}

void GameLoopEditorPanelInternal::DrawLoadingPolicyEditor(GameLoopTransition& transition)
{
    GameLoopLoadingPolicy& policy = transition.loadingPolicy;

    int mode = 0;
    if (policy.mode == GameLoopLoadingMode::FadeOnly) mode = 1;
    else if (policy.mode == GameLoopLoadingMode::LoadingOverlay) mode = 2;

    const char* modes[] = { "Immediate", "Fade Only", "Loading Overlay" };
    if (ImGui::Combo("Mode", &mode, modes, 3)) {
        policy.mode = mode == 1 ? GameLoopLoadingMode::FadeOnly :
            (mode == 2 ? GameLoopLoadingMode::LoadingOverlay : GameLoopLoadingMode::Immediate);
        m_dirty = true;
    }
    if (ImGui::DragFloat("Fade Out", &policy.fadeOutSeconds, 0.01f, 0.0f, 30.0f, "%.2f")) {
        m_dirty = true;
    }
    if (ImGui::DragFloat("Fade In", &policy.fadeInSeconds, 0.01f, 0.0f, 30.0f, "%.2f")) {
        m_dirty = true;
    }
    if (ImGui::DragFloat("Min Loading", &policy.minimumLoadingSeconds, 0.01f, 0.0f, 60.0f, "%.2f")) {
        m_dirty = true;
    }
    if (DrawStringInput("Message", policy.loadingMessage)) {
        m_dirty = true;
    }
    if (ImGui::Checkbox("Block Input", &policy.blockInput)) {
        m_dirty = true;
    }
}

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
