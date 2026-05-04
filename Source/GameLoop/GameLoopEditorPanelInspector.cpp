#include "GameLoopEditorPanelInternal.h"

#include "Component/HierarchyComponent.h"
#include "Component/NameComponent.h"
#include "Component/UIButtonComponent.h"
#include "Engine/EditorSelection.h"
#include "Engine/EngineKernel.h"

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

    struct StringOption
    {
        const char* value;
        const char* label;
    };

    struct UIButtonOption
    {
        EntityID entity = Entity::NULL_ID;
        std::string label;
        std::string buttonId;
        bool enabled = true;
        bool active = true;
    };

    const char* NodeTypeLabel(GameLoopNodeType type)
    {
        switch (type) {
        case GameLoopNodeType::Scene: return "Scene";
        case GameLoopNodeType::State: return "State";
        case GameLoopNodeType::Event: return "Event";
        case GameLoopNodeType::Action:return "Action";
        case GameLoopNodeType::Battle:return "Battle";
        }
        return "State";
    }

    std::string BuildInspectorSceneName(const GameLoopNode& node)
    {
        if (node.type == GameLoopNodeType::Scene && !node.scenePath.empty()) {
            return GameLoopScenePicker::BuildNodeNameFromScenePath(node.scenePath);
        }
        if (!node.name.empty()) {
            return node.name;
        }
        return NodeTypeLabel(node.type);
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

    const StringOption* GetEventCatalog(int& outCount)
    {
        static const StringOption options[] = {
            { "ui.button.clicked", "ui.button.clicked" },
            { "input.action.pressed", "input.action.pressed" },
            { "input.keyboard.down", "input.keyboard.down" },
            { "input.gamepad.down", "input.gamepad.down" },
            { "scene.loaded", "scene.loaded" },
            { "battle.start.requested", "battle.start.requested" },
            { "battle.reset.requested", "battle.reset.requested" },
            { "battle.ended", "battle.ended" },
            { "battle.result", "battle.result" },
            { "battle.victory", "battle.victory" },
            { "battle.defeat", "battle.defeat" },
            { "flow.flag.changed", "flow.flag.changed" },
            { "flow.wait.started", "flow.wait.started" },
            { "flow.fade.started", "flow.fade.started" },
            { "loading.overlay.shown", "loading.overlay.shown" },
            { "loading.overlay.hidden", "loading.overlay.hidden" },
            { "flow.custom", "flow.custom" },
        };
        outCount = sizeof(options) / sizeof(options[0]);
        return options;
    }

    const StringOption* GetInputActionCatalog(int& outCount)
    {
        static const StringOption options[] = {
            { "Submit", "Submit" },
            { "Cancel", "Cancel" },
            { "Attack", "Attack" },
            { "Jump", "Jump" },
            { "Dash", "Dash" },
            { "Interact", "Interact" },
            { "Pause", "Pause" },
        };
        outCount = sizeof(options) / sizeof(options[0]);
        return options;
    }

    std::string BuildEntityLabel(Registry* registry, EntityID entity)
    {
        if (registry) {
            if (auto* name = registry->GetComponent<NameComponent>(entity)) {
                if (!name->name.empty()) {
                    return name->name;
                }
            }
        }
        return "Entity " + std::to_string(Entity::GetIndex(entity));
    }

    std::vector<UIButtonOption> CollectUIButtonOptions(Registry* registry)
    {
        std::vector<UIButtonOption> options;
        if (!registry) {
            return options;
        }

        const ComponentTypeID buttonType = TypeManager::GetComponentTypeID<UIButtonComponent>();
        const ComponentTypeID nameType = TypeManager::GetComponentTypeID<NameComponent>();
        const ComponentTypeID hierarchyType = TypeManager::GetComponentTypeID<HierarchyComponent>();

        for (Archetype* archetype : registry->GetAllArchetypes()) {
            if (!archetype) {
                continue;
            }
            const Signature signature = archetype->GetSignature();
            if (!signature.test(buttonType)) {
                continue;
            }

            ComponentColumn* buttonColumn = archetype->GetColumn(buttonType);
            ComponentColumn* nameColumn = signature.test(nameType) ? archetype->GetColumn(nameType) : nullptr;
            ComponentColumn* hierarchyColumn = signature.test(hierarchyType) ? archetype->GetColumn(hierarchyType) : nullptr;
            if (!buttonColumn) {
                continue;
            }

            const auto& entities = archetype->GetEntities();
            for (size_t i = 0; i < archetype->GetEntityCount() && i < entities.size(); ++i) {
                auto* button = static_cast<UIButtonComponent*>(buttonColumn->Get(i));
                if (!button) {
                    continue;
                }

                std::string entityName = "Entity " + std::to_string(Entity::GetIndex(entities[i]));
                if (nameColumn) {
                    auto* name = static_cast<NameComponent*>(nameColumn->Get(i));
                    if (name && !name->name.empty()) {
                        entityName = name->name;
                    }
                }

                bool active = true;
                if (hierarchyColumn) {
                    auto* hierarchy = static_cast<HierarchyComponent*>(hierarchyColumn->Get(i));
                    active = !hierarchy || hierarchy->isActive;
                }

                UIButtonOption option;
                option.entity = entities[i];
                option.buttonId = button->buttonId;
                option.enabled = button->enabled;
                option.active = active;
                option.label = entityName + "  ->  " + (button->buttonId.empty() ? std::string{ "(empty event)" } : button->buttonId);
                if (!button->enabled) {
                    option.label += " [disabled]";
                }
                if (!active) {
                    option.label += " [inactive]";
                }
                options.push_back(std::move(option));
            }
        }

        std::sort(options.begin(), options.end(),
            [](const UIButtonOption& a, const UIButtonOption& b) {
                return a.label < b.label;
            });
        return options;
    }

    const GamepadButtonOption* GetGamepadButtonOptions(int& outCount)
    {
        static const GamepadButtonOption options[] = {
            { kGameFlowUnboundGamepadButton, "Unbound" },
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

    bool DrawStringCatalogCombo(const char* label, std::string& value, const StringOption* options, int count)
    {
        bool changed = false;
        const char* preview = value.empty() ? "(select)" : value.c_str();
        if (ImGui::BeginCombo(label, preview)) {
            for (int i = 0; i < count; ++i) {
                const bool selected = value == options[i].value;
                if (ImGui::Selectable(options[i].label, selected)) {
                    value = options[i].value;
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

    bool DrawNodeTypeCombo(GameLoopNodeType& type)
    {
        static const GameLoopNodeType types[] = {
            GameLoopNodeType::Scene,
            GameLoopNodeType::State,
            GameLoopNodeType::Event,
            GameLoopNodeType::Action,
            GameLoopNodeType::Battle,
        };

        bool changed = false;
        if (ImGui::BeginCombo("Type", NodeTypeLabel(type))) {
            for (GameLoopNodeType candidate : types) {
                const bool selected = candidate == type;
                if (ImGui::Selectable(NodeTypeLabel(candidate), selected)) {
                    type = candidate;
                    changed = true;
                }
                if (selected) ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }
        return changed;
    }

    const char* ConditionTypeLabel(GameFlowConditionType type)
    {
        switch (type) {
        case GameFlowConditionType::Event: return "Event";
        case GameFlowConditionType::InputAction: return "Input Action";
        case GameFlowConditionType::UIButtonClick: return "UI Button Click";
        case GameFlowConditionType::TimerElapsed: return "Timer Elapsed";
        case GameFlowConditionType::FlagEquals: return "Flag Equals";
        case GameFlowConditionType::BattleResult: return "Battle Result";
        case GameFlowConditionType::SceneLoaded: return "Scene Loaded";
        }
        return "Event";
    }

    const char* ActionTypeLabel(GameFlowActionType type)
    {
        switch (type) {
        case GameFlowActionType::LoadScene: return "Load Scene";
        case GameFlowActionType::SetCurrentNode: return "Set Current Node";
        case GameFlowActionType::EmitEvent: return "Emit Event";
        case GameFlowActionType::SetFlag: return "Set Flag";
        case GameFlowActionType::ClearFlag: return "Clear Flag";
        case GameFlowActionType::StartBattleFlow: return "Start BattleFlow";
        case GameFlowActionType::ResetBattleFlow: return "Reset BattleFlow";
        case GameFlowActionType::Fade: return "Fade";
        case GameFlowActionType::Wait: return "Wait";
        case GameFlowActionType::ShowLoadingOverlay: return "Show Loading";
        case GameFlowActionType::HideLoadingOverlay: return "Hide Loading";
        }
        return "Load Scene";
    }

    bool DrawConditionTypeCombo(GameFlowConditionType& type)
    {
        static const GameFlowConditionType types[] = {
            GameFlowConditionType::Event,
            GameFlowConditionType::InputAction,
            GameFlowConditionType::UIButtonClick,
            GameFlowConditionType::TimerElapsed,
            GameFlowConditionType::FlagEquals,
            GameFlowConditionType::BattleResult,
            GameFlowConditionType::SceneLoaded,
        };

        bool changed = false;
        if (ImGui::BeginCombo("Type", ConditionTypeLabel(type))) {
            for (GameFlowConditionType candidate : types) {
                const bool selected = candidate == type;
                if (ImGui::Selectable(ConditionTypeLabel(candidate), selected)) {
                    type = candidate;
                    changed = true;
                }
                if (selected) ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }
        return changed;
    }

    bool DrawActionTypeCombo(GameFlowActionType& type)
    {
        static const GameFlowActionType types[] = {
            GameFlowActionType::LoadScene,
            GameFlowActionType::SetCurrentNode,
            GameFlowActionType::EmitEvent,
            GameFlowActionType::SetFlag,
            GameFlowActionType::ClearFlag,
            GameFlowActionType::StartBattleFlow,
            GameFlowActionType::ResetBattleFlow,
            GameFlowActionType::Fade,
            GameFlowActionType::Wait,
            GameFlowActionType::ShowLoadingOverlay,
            GameFlowActionType::HideLoadingOverlay,
        };

        bool changed = false;
        if (ImGui::BeginCombo("Type", ActionTypeLabel(type))) {
            for (GameFlowActionType candidate : types) {
                const bool selected = candidate == type;
                if (ImGui::Selectable(ActionTypeLabel(candidate), selected)) {
                    type = candidate;
                    changed = true;
                }
                if (selected) ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }
        return changed;
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

        if (DrawNodeTypeCombo(node->type)) {
            if (node->type != GameLoopNodeType::Scene) {
                node->scenePath.clear();
                if (node->name.empty()) {
                    node->name = NodeTypeLabel(node->type);
                }
            }
            m_dirty = true;
        }

        if (DrawStringInput("Name", node->name)) {
            m_dirty = true;
        }

        const std::string displayName = BuildInspectorSceneName(*node);
        ImGui::Text("Display: %s", displayName.c_str());
        ImGui::Text("Id: %u", node->id);
        ImGui::Text("Start: %s", node->id == m_asset.startNodeId ? "Yes" : "No");

        if (node->type == GameLoopNodeType::Scene) {
            DrawReadOnlyScenePath(*node);

            std::string dropped;
            if (m_scenePicker.AcceptSceneAssetDragDrop(dropped)) {
                ReplaceNodeScene(node->id, dropped);
            }

            if (ImGui::Button("Replace Scene")) {
                OpenPickerForReplace(node->id);
            }
            ImGui::SameLine();
        }

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

        if (ImGui::InputInt("Priority", &transition->priority)) {
            m_dirty = true;
        }

        int mode = transition->conditionMode == GameFlowConditionMode::Any ? 1 : 0;
        const char* modes[] = { "All", "Any" };
        if (ImGui::Combo("Condition Mode", &mode, modes, 2)) {
            transition->conditionMode = mode == 1 ? GameFlowConditionMode::Any : GameFlowConditionMode::All;
            m_dirty = true;
        }

        ImGui::Spacing();
        if (ImGui::CollapsingHeader("Conditions", ImGuiTreeNodeFlags_DefaultOpen)) {
            int removeIndex = -1;
            for (int i = 0; i < static_cast<int>(transition->conditions.size()); ++i) {
                ImGui::PushID(i);
                GameFlowCondition& condition = transition->conditions[i];
                if (ImGui::TreeNodeEx("Condition", ImGuiTreeNodeFlags_DefaultOpen, "Condition %d: %s", i + 1, ConditionTypeLabel(condition.type))) {
                    DrawConditionEditor(condition, i);
                    if (ImGui::Button("Remove Condition")) {
                        removeIndex = i;
                    }
                    ImGui::TreePop();
                }
                ImGui::PopID();
            }
            if (removeIndex >= 0) {
                transition->conditions.erase(transition->conditions.begin() + removeIndex);
                m_dirty = true;
            }

            if (ImGui::Button("Add UI Button")) {
                GameFlowCondition condition;
                condition.type = GameFlowConditionType::UIButtonClick;
                transition->conditions.push_back(condition);
                m_dirty = true;
            }
            ImGui::SameLine();
            if (ImGui::Button("Add Input")) {
                GameFlowCondition condition;
                condition.type = GameFlowConditionType::InputAction;
                transition->conditions.push_back(condition);
                m_dirty = true;
            }
            ImGui::SameLine();
            if (ImGui::Button("Add Battle")) {
                GameFlowCondition condition;
                condition.type = GameFlowConditionType::BattleResult;
                condition.value = "Victory";
                transition->conditions.push_back(condition);
                m_dirty = true;
            }
        }

        ImGui::Spacing();
        if (ImGui::CollapsingHeader("Actions", ImGuiTreeNodeFlags_DefaultOpen)) {
            int removeIndex = -1;
            for (int i = 0; i < static_cast<int>(transition->actions.size()); ++i) {
                ImGui::PushID(i);
                GameFlowAction& action = transition->actions[i];
                if (ImGui::TreeNodeEx("Action", ImGuiTreeNodeFlags_DefaultOpen, "Action %d: %s", i + 1, ActionTypeLabel(action.type))) {
                    DrawActionEditor(action, i, toNode);
                    if (ImGui::Button("Remove Action")) {
                        removeIndex = i;
                    }
                    ImGui::TreePop();
                }
                ImGui::PopID();
            }
            if (removeIndex >= 0) {
                transition->actions.erase(transition->actions.begin() + removeIndex);
                m_dirty = true;
            }

            if (ImGui::Button("Add Load Scene")) {
                GameFlowAction action;
                action.type = GameFlowActionType::LoadScene;
                if (toNode && toNode->type == GameLoopNodeType::Scene) action.target = toNode->scenePath;
                transition->actions.push_back(action);
                m_dirty = true;
            }
            ImGui::SameLine();
            if (ImGui::Button("Add Set Node")) {
                GameFlowAction action;
                action.type = GameFlowActionType::SetCurrentNode;
                transition->actions.push_back(action);
                m_dirty = true;
            }
            ImGui::SameLine();
            if (ImGui::Button("Add Event")) {
                GameFlowAction action;
                action.type = GameFlowActionType::EmitEvent;
                transition->actions.push_back(action);
                m_dirty = true;
            }
            if (ImGui::Button("Add Wait")) {
                GameFlowAction action;
                action.type = GameFlowActionType::Wait;
                action.seconds = 0.5f;
                transition->actions.push_back(action);
                m_dirty = true;
            }
            ImGui::SameLine();
            if (ImGui::Button("Add Fade")) {
                GameFlowAction action;
                action.type = GameFlowActionType::Fade;
                action.seconds = 0.5f;
                transition->actions.push_back(action);
                m_dirty = true;
            }
            ImGui::SameLine();
            if (ImGui::Button("Add Battle Start")) {
                GameFlowAction action;
                action.type = GameFlowActionType::StartBattleFlow;
                action.target = toNode && toNode->type == GameLoopNodeType::Battle && !toNode->name.empty()
                    ? toNode->name
                    : "default";
                transition->actions.push_back(action);
                m_dirty = true;
            }
            if (ImGui::Button("Add Show Loading")) {
                GameFlowAction action;
                action.type = GameFlowActionType::ShowLoadingOverlay;
                action.message = "Loading...";
                transition->actions.push_back(action);
                m_dirty = true;
            }
            ImGui::SameLine();
            if (ImGui::Button("Add Hide Loading")) {
                GameFlowAction action;
                action.type = GameFlowActionType::HideLoadingOverlay;
                transition->actions.push_back(action);
                m_dirty = true;
            }
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
        return;
    }

    ImGui::TextDisabled("No Selection");
}

void GameLoopEditorPanelInternal::DrawConditionEditor(GameFlowCondition& condition, int index)
{
    (void)index;
    if (DrawConditionTypeCombo(condition.type)) {
        m_dirty = true;
    }

    switch (condition.type) {
    case GameFlowConditionType::Event:
        {
            int count = 0;
            const StringOption* options = GetEventCatalog(count);
            if (DrawStringCatalogCombo("Known Event", condition.name, options, count)) m_dirty = true;
        }
        if (DrawStringInput("Event Name", condition.name)) m_dirty = true;
        if (DrawStringInput("Value", condition.value)) m_dirty = true;
        break;

    case GameFlowConditionType::InputAction:
        if (DrawKeyboardCombo(condition.keyboardScancode)) m_dirty = true;
        if (DrawGamepadCombo(condition.gamepadButton)) m_dirty = true;
        {
            int count = 0;
            const StringOption* options = GetInputActionCatalog(count);
            if (DrawStringCatalogCombo("Known Action", condition.value, options, count)) m_dirty = true;
        }
        if (DrawStringInput("Action Name", condition.value)) m_dirty = true;
        break;

    case GameFlowConditionType::UIButtonClick:
        {
            Registry* registry = EngineKernel::Instance().GetGameRegistry();
            std::vector<UIButtonOption> buttons = CollectUIButtonOptions(registry);
            const char* preview = condition.value.empty() ? "(select UIButton)" : condition.value.c_str();

            ImGui::TextDisabled("Consumes UIButtonComponent Event Name emitted as ui.button.clicked.");
            if (ImGui::BeginCombo("Scene UIButton", preview)) {
                if (buttons.empty()) {
                    ImGui::TextDisabled("No UIButtonComponent in the current scene.");
                }
                for (const UIButtonOption& button : buttons) {
                    const bool selectable = !button.buttonId.empty();
                    const bool selected = !condition.value.empty() && condition.value == button.buttonId;
                    if (!selectable) {
                        ImGui::BeginDisabled();
                    }
                    if (ImGui::Selectable(button.label.c_str(), selected)) {
                        condition.value = button.buttonId;
                        m_dirty = true;
                    }
                    if (!selectable) {
                        ImGui::EndDisabled();
                    }
                    if (selected) {
                        ImGui::SetItemDefaultFocus();
                    }
                }
                ImGui::EndCombo();
            }

            EntityID selectedEntity = EditorSelection::Instance().GetPrimaryEntity();
            UIButtonComponent* selectedButton = registry && !Entity::IsNull(selectedEntity)
                ? registry->GetComponent<UIButtonComponent>(selectedEntity)
                : nullptr;
            const bool canUseSelected = selectedButton && !selectedButton->buttonId.empty();
            if (!canUseSelected) {
                ImGui::BeginDisabled();
            }
            if (ImGui::Button("Use Selected Hierarchy UIButton")) {
                condition.value = selectedButton->buttonId;
                m_dirty = true;
            }
            if (!canUseSelected) {
                ImGui::EndDisabled();
            }

            if (selectedButton) {
                ImGui::TextDisabled("Selected: %s -> %s",
                    BuildEntityLabel(registry, selectedEntity).c_str(),
                    selectedButton->buttonId.empty() ? "(empty event)" : selectedButton->buttonId.c_str());
            }
            else {
                ImGui::TextDisabled("Select a UIButton entity in Hierarchy to attach it here.");
            }

            if (condition.value.empty()) {
                ImGui::BeginDisabled();
            }
            if (ImGui::Button("Select Matching Button")) {
                for (const UIButtonOption& button : buttons) {
                    if (!condition.value.empty() && condition.value == button.buttonId) {
                        EditorSelection::Instance().SelectEntity(button.entity);
                        break;
                    }
                }
            }
            if (condition.value.empty()) {
                ImGui::EndDisabled();
            }

            if (condition.value.empty()) {
                ImGui::TextColored(ImVec4(1.0f, 0.74f, 0.28f, 1.0f),
                    "Pick a UIButton from the current scene.");
            }
        }
        break;

    case GameFlowConditionType::TimerElapsed:
        if (ImGui::DragFloat("Seconds", &condition.seconds, 0.01f, 0.0f, 3600.0f, "%.2f")) m_dirty = true;
        break;

    case GameFlowConditionType::FlagEquals:
        if (DrawStringInput("Flag", condition.name)) m_dirty = true;
        if (ImGui::Checkbox("Expected", &condition.expectedFlagValue)) m_dirty = true;
        break;

    case GameFlowConditionType::BattleResult: {
        const char* results[] = { "Victory", "Defeat" };
        int result = condition.value == "Defeat" ? 1 : 0;
        if (ImGui::Combo("Result", &result, results, 2)) {
            condition.value = results[result];
            m_dirty = true;
        }
        break;
    }

    case GameFlowConditionType::SceneLoaded:
        if (DrawStringInput("Scene Path", condition.value)) m_dirty = true;
        break;
    }
}

void GameLoopEditorPanelInternal::DrawActionEditor(GameFlowAction& action, int index, const GameLoopNode* toNode)
{
    (void)index;
    if (DrawActionTypeCombo(action.type)) {
        m_dirty = true;
    }

    switch (action.type) {
    case GameFlowActionType::LoadScene:
        if (DrawStringInput("Scene Path", action.target)) m_dirty = true;
        if (toNode && toNode->type == GameLoopNodeType::Scene && ImGui::Button("Use To Node Scene")) {
            action.target = toNode->scenePath;
            m_dirty = true;
        }
        break;

    case GameFlowActionType::SetCurrentNode:
        ImGui::TextDisabled("Uses this transition's To node.");
        break;

    case GameFlowActionType::EmitEvent:
        {
            int count = 0;
            const StringOption* options = GetEventCatalog(count);
            if (DrawStringCatalogCombo("Known Event", action.target, options, count)) m_dirty = true;
        }
        if (DrawStringInput("Event Name", action.target)) m_dirty = true;
        if (DrawStringInput("Value", action.value)) m_dirty = true;
        break;

    case GameFlowActionType::SetFlag:
        if (DrawStringInput("Flag", action.target)) m_dirty = true;
        if (ImGui::Checkbox("Value", &action.boolValue)) m_dirty = true;
        break;

    case GameFlowActionType::ClearFlag:
        if (DrawStringInput("Flag", action.target)) m_dirty = true;
        break;

    case GameFlowActionType::StartBattleFlow:
        if (DrawStringInput("Battle Id", action.target)) m_dirty = true;
        break;

    case GameFlowActionType::ResetBattleFlow:
        ImGui::TextDisabled("No parameters.");
        break;

    case GameFlowActionType::Fade:
    case GameFlowActionType::Wait:
        if (ImGui::DragFloat("Seconds", &action.seconds, 0.01f, 0.0f, 60.0f, "%.2f")) m_dirty = true;
        break;

    case GameFlowActionType::ShowLoadingOverlay:
        if (DrawStringInput("Message", action.message)) m_dirty = true;
        break;

    case GameFlowActionType::HideLoadingOverlay:
        ImGui::TextDisabled("No parameters.");
        break;
    }
}

void GameLoopEditorPanelInternal::DrawValidateSummary()
{
    const auto drawRecentEvents = []() {
        if (ImGui::TreeNode("Recent Events")) {
            const auto& events = EngineKernel::Instance().GetFlowEventQueue().GetRecentEvents();
            if (events.empty()) {
                ImGui::TextDisabled("(none)");
            }
            else {
                const int total = static_cast<int>(events.size());
                const int first = total > 16 ? total - 16 : 0;
                for (int i = first; i < total; ++i) {
                    const FlowEvent& event = events[static_cast<size_t>(i)];
                    if (event.value.empty()) {
                        ImGui::Text("%s", event.name.c_str());
                    }
                    else {
                        ImGui::Text("%s : %s", event.name.c_str(), event.value.c_str());
                    }
                }
            }
            ImGui::TreePop();
        }
    };

    if (!m_validated) {
        ImGui::TextDisabled("Validate has not been run.");
        drawRecentEvents();
        return;
    }

    ImGui::Text("Errors: %d  Warnings: %d", m_validateResult.ErrorCount(), m_validateResult.WarningCount());

    if (ImGui::TreeNode("Details")) {
        for (const auto& message : m_validateResult.messages) {
            ImGui::TextWrapped("%s", message.message.c_str());
        }
        ImGui::TreePop();
    }

    drawRecentEvents();
}
