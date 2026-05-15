#include "InputMappingTab.h"
#include <cstdio>
#include <cstring>
#include <imgui.h>
#include "Icon/IconsFontAwesome7.h"
#include "Input/InputActionMapComponent.h"
#include "Input/ResolvedInputStateComponent.h"
#include "Archetype/Archetype.h"
#include "Component/ComponentSignature.h"
#include "Registry/Registry.h"
#include "Type/TypeInfo.h"

namespace
{
    const char* FormatKeyboardLabel(uint32_t scancode)
    {
        switch (scancode) {
        case 0: return "None";
        case 4: return "A"; case 5: return "B"; case 6: return "C"; case 7: return "D";
        case 8: return "E"; case 9: return "F"; case 10: return "G"; case 11: return "H";
        case 12: return "I"; case 13: return "J"; case 14: return "K"; case 15: return "L";
        case 16: return "M"; case 17: return "N"; case 18: return "O"; case 19: return "P";
        case 20: return "Q"; case 21: return "R"; case 22: return "S"; case 23: return "T";
        case 24: return "U"; case 25: return "V"; case 26: return "W"; case 27: return "X";
        case 28: return "Y"; case 29: return "Z";
        case 30: return "1"; case 31: return "2"; case 32: return "3"; case 33: return "4";
        case 34: return "5"; case 35: return "6"; case 36: return "7"; case 37: return "8";
        case 38: return "9"; case 39: return "0";
        case 40: return "Enter"; case 41: return "Esc"; case 42: return "Backspace";
        case 43: return "Tab"; case 44: return "Space";
        case 45: return "-"; case 46: return "="; case 47: return "["; case 48: return "]";
        case 49: return "\\"; case 51: return ";"; case 52: return "'"; case 53: return "`";
        case 54: return ","; case 55: return "."; case 56: return "/";
        case 57: return "CapsLock";
        case 58: return "F1"; case 59: return "F2"; case 60: return "F3"; case 61: return "F4";
        case 62: return "F5"; case 63: return "F6"; case 64: return "F7"; case 65: return "F8";
        case 66: return "F9"; case 67: return "F10"; case 68: return "F11"; case 69: return "F12";
        case 76: return "Delete"; case 79: return "Right"; case 80: return "Left";
        case 81: return "Down"; case 82: return "Up";
        case 224: return "Ctrl L"; case 225: return "Shift L"; case 226: return "Alt L";
        case 228: return "Ctrl R"; case 229: return "Shift R"; case 230: return "Alt R";
        default: break;
        }

        static char buffer[32];
        std::snprintf(buffer, sizeof(buffer), "Key %u", scancode);
        return buffer;
    }

    const char* FormatMouseButtonLabel(uint8_t button)
    {
        switch (button) {
        case 0: return "None";
        case 1: return "Left";
        case 2: return "Middle";
        case 3: return "Right";
        case 4: return "Back";
        case 5: return "Forward";
        default: break;
        }

        static char buffer[32];
        std::snprintf(buffer, sizeof(buffer), "Button %u", button);
        return buffer;
    }

    const char* FormatGamepadButtonLabel(uint8_t button)
    {
        switch (button) {
        case 0xFF: return "None";
        case 0: return "A"; case 1: return "B"; case 2: return "X"; case 3: return "Y";
        case 4: return "Back"; case 5: return "Guide"; case 6: return "Start";
        case 7: return "L3"; case 8: return "R3"; case 9: return "LB"; case 10: return "RB";
        case 11: return "DPad Up"; case 12: return "DPad Down";
        case 13: return "DPad Left"; case 14: return "DPad Right";
        default: break;
        }

        static char buffer[32];
        std::snprintf(buffer, sizeof(buffer), "Button %u", button);
        return buffer;
    }

    const char* FormatGamepadAxisLabel(uint8_t axis)
    {
        switch (axis) {
        case 0xFF: return "None";
        case 0: return "Left Stick X";
        case 1: return "Left Stick Y";
        case 2: return "Right Stick X";
        case 3: return "Right Stick Y";
        case 4: return "LT";
        case 5: return "RT";
        default: break;
        }

        static char buffer[32];
        std::snprintf(buffer, sizeof(buffer), "Axis %u", axis);
        return buffer;
    }

    bool TryMapImGuiKeyToSdlScancode(ImGuiKey key, uint32_t& outScancode)
    {
        switch (key) {
        case ImGuiKey_A: outScancode = 4; return true;
        case ImGuiKey_B: outScancode = 5; return true;
        case ImGuiKey_C: outScancode = 6; return true;
        case ImGuiKey_D: outScancode = 7; return true;
        case ImGuiKey_E: outScancode = 8; return true;
        case ImGuiKey_F: outScancode = 9; return true;
        case ImGuiKey_G: outScancode = 10; return true;
        case ImGuiKey_H: outScancode = 11; return true;
        case ImGuiKey_I: outScancode = 12; return true;
        case ImGuiKey_J: outScancode = 13; return true;
        case ImGuiKey_K: outScancode = 14; return true;
        case ImGuiKey_L: outScancode = 15; return true;
        case ImGuiKey_M: outScancode = 16; return true;
        case ImGuiKey_N: outScancode = 17; return true;
        case ImGuiKey_O: outScancode = 18; return true;
        case ImGuiKey_P: outScancode = 19; return true;
        case ImGuiKey_Q: outScancode = 20; return true;
        case ImGuiKey_R: outScancode = 21; return true;
        case ImGuiKey_S: outScancode = 22; return true;
        case ImGuiKey_T: outScancode = 23; return true;
        case ImGuiKey_U: outScancode = 24; return true;
        case ImGuiKey_V: outScancode = 25; return true;
        case ImGuiKey_W: outScancode = 26; return true;
        case ImGuiKey_X: outScancode = 27; return true;
        case ImGuiKey_Y: outScancode = 28; return true;
        case ImGuiKey_Z: outScancode = 29; return true;
        case ImGuiKey_1: outScancode = 30; return true;
        case ImGuiKey_2: outScancode = 31; return true;
        case ImGuiKey_3: outScancode = 32; return true;
        case ImGuiKey_4: outScancode = 33; return true;
        case ImGuiKey_5: outScancode = 34; return true;
        case ImGuiKey_6: outScancode = 35; return true;
        case ImGuiKey_7: outScancode = 36; return true;
        case ImGuiKey_8: outScancode = 37; return true;
        case ImGuiKey_9: outScancode = 38; return true;
        case ImGuiKey_0: outScancode = 39; return true;
        case ImGuiKey_Enter: outScancode = 40; return true;
        case ImGuiKey_Escape: outScancode = 41; return true;
        case ImGuiKey_Backspace: outScancode = 42; return true;
        case ImGuiKey_Tab: outScancode = 43; return true;
        case ImGuiKey_Space: outScancode = 44; return true;
        case ImGuiKey_Minus: outScancode = 45; return true;
        case ImGuiKey_Equal: outScancode = 46; return true;
        case ImGuiKey_LeftBracket: outScancode = 47; return true;
        case ImGuiKey_RightBracket: outScancode = 48; return true;
        case ImGuiKey_Backslash: outScancode = 49; return true;
        case ImGuiKey_Semicolon: outScancode = 51; return true;
        case ImGuiKey_Apostrophe: outScancode = 52; return true;
        case ImGuiKey_GraveAccent: outScancode = 53; return true;
        case ImGuiKey_Comma: outScancode = 54; return true;
        case ImGuiKey_Period: outScancode = 55; return true;
        case ImGuiKey_Slash: outScancode = 56; return true;
        case ImGuiKey_CapsLock: outScancode = 57; return true;
        case ImGuiKey_F1: outScancode = 58; return true;
        case ImGuiKey_F2: outScancode = 59; return true;
        case ImGuiKey_F3: outScancode = 60; return true;
        case ImGuiKey_F4: outScancode = 61; return true;
        case ImGuiKey_F5: outScancode = 62; return true;
        case ImGuiKey_F6: outScancode = 63; return true;
        case ImGuiKey_F7: outScancode = 64; return true;
        case ImGuiKey_F8: outScancode = 65; return true;
        case ImGuiKey_F9: outScancode = 66; return true;
        case ImGuiKey_F10: outScancode = 67; return true;
        case ImGuiKey_F11: outScancode = 68; return true;
        case ImGuiKey_F12: outScancode = 69; return true;
        case ImGuiKey_Delete: outScancode = 76; return true;
        case ImGuiKey_RightArrow: outScancode = 79; return true;
        case ImGuiKey_LeftArrow: outScancode = 80; return true;
        case ImGuiKey_DownArrow: outScancode = 81; return true;
        case ImGuiKey_UpArrow: outScancode = 82; return true;
        case ImGuiKey_LeftCtrl: outScancode = 224; return true;
        case ImGuiKey_LeftShift: outScancode = 225; return true;
        case ImGuiKey_LeftAlt: outScancode = 226; return true;
        case ImGuiKey_RightCtrl: outScancode = 228; return true;
        case ImGuiKey_RightShift: outScancode = 229; return true;
        case ImGuiKey_RightAlt: outScancode = 230; return true;
        default: return false;
        }
    }

    bool TryCaptureMouseButton(uint8_t& outButton)
    {
        if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) { outButton = 1; return true; }
        if (ImGui::IsMouseClicked(ImGuiMouseButton_Right)) { outButton = 3; return true; }
        if (ImGui::IsMouseClicked(ImGuiMouseButton_Middle)) { outButton = 2; return true; }
        return false;
    }

    bool TryCaptureGamepadButton(uint8_t& outButton)
    {
        struct ButtonMap { ImGuiKey key; uint8_t button; };
        static constexpr ButtonMap maps[] = {
            { ImGuiKey_GamepadFaceDown, 0 },
            { ImGuiKey_GamepadFaceRight, 1 },
            { ImGuiKey_GamepadFaceLeft, 2 },
            { ImGuiKey_GamepadFaceUp, 3 },
            { ImGuiKey_GamepadBack, 4 },
            { ImGuiKey_GamepadStart, 6 },
            { ImGuiKey_GamepadL3, 7 },
            { ImGuiKey_GamepadR3, 8 },
            { ImGuiKey_GamepadL1, 9 },
            { ImGuiKey_GamepadR1, 10 },
            { ImGuiKey_GamepadDpadUp, 11 },
            { ImGuiKey_GamepadDpadDown, 12 },
            { ImGuiKey_GamepadDpadLeft, 13 },
            { ImGuiKey_GamepadDpadRight, 14 },
        };

        for (const auto& map : maps) {
            if (ImGui::IsKeyPressed(map.key)) {
                outButton = map.button;
                return true;
            }
        }
        return false;
    }

    bool TryCaptureGamepadAxis(uint8_t& outAxis)
    {
        struct AxisMap { ImGuiKey key; uint8_t axis; };
        static constexpr AxisMap maps[] = {
            { ImGuiKey_GamepadLStickLeft, 0 },
            { ImGuiKey_GamepadLStickRight, 0 },
            { ImGuiKey_GamepadLStickUp, 1 },
            { ImGuiKey_GamepadLStickDown, 1 },
            { ImGuiKey_GamepadRStickLeft, 2 },
            { ImGuiKey_GamepadRStickRight, 2 },
            { ImGuiKey_GamepadRStickUp, 3 },
            { ImGuiKey_GamepadRStickDown, 3 },
            { ImGuiKey_GamepadL2, 4 },
            { ImGuiKey_GamepadR2, 5 },
        };

        for (const auto& map : maps) {
            if (ImGui::IsKeyPressed(map.key)) {
                outAxis = map.axis;
                return true;
            }
        }
        return false;
    }

    const char* GetCapturePrompt(InputMappingTab::CaptureField field)
    {
        switch (field) {
        case InputMappingTab::CaptureField::ActionMouse:
            return "Press a mouse button";
        case InputMappingTab::CaptureField::ActionGamepad:
            return "Press a gamepad button";
        case InputMappingTab::CaptureField::AxisGamepad:
            return "Move a gamepad axis";
        default:
            return "Press a keyboard key";
        }
    }
}

void InputMappingTab::SetEditingMap(const InputActionMapAsset& map)
{
    m_editingMap = map;
    m_dirty = false;
}

void InputMappingTab::ClearEditingMap()
{
    m_editingMap = InputActionMapAsset{};
    m_dirty = false;
}

void InputMappingTab::Draw(Registry* registry)
{
    if (!m_editingMap.name.empty()) {
        ImGui::TextDisabled("%s", m_editingMap.name.c_str());
        if (m_dirty) {
            ImGui::SameLine(0.0f, 6.0f);
            ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.15f, 1.0f), "*");
        }
        ImGui::Separator();
    }

    if (ImGui::BeginTabBar("InputSubTabs")) {
        if (ImGui::BeginTabItem("Actions")) {
            DrawActionTable();
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Axes")) {
            DrawAxisTable();
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Settings")) {
            DrawSettings();
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem(ICON_FA_GAMEPAD " Live Test")) {
            DrawLiveTest(registry);
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }

    DrawKeyBindPopup();
}

void InputMappingTab::DrawActionTable()
{
    if (ImGui::BeginTable("ActionsTable", 6,
        ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable))
    {
        ImGui::TableSetupColumn("Action", ImGuiTableColumnFlags_WidthFixed, 120);
        ImGui::TableSetupColumn("Keyboard", ImGuiTableColumnFlags_WidthFixed, 90);
        ImGui::TableSetupColumn("Mouse", ImGuiTableColumnFlags_WidthFixed, 70);
        ImGui::TableSetupColumn("Gamepad", ImGuiTableColumnFlags_WidthFixed, 100);
        ImGui::TableSetupColumn("Trigger", ImGuiTableColumnFlags_WidthFixed, 80);
        ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthFixed, 30);
        ImGui::TableHeadersRow();

        for (int i = 0; i < (int)m_editingMap.actions.size(); ++i) {
            auto& action = m_editingMap.actions[i];
            ImGui::PushID(i);
            ImGui::TableNextRow();

            // Action name.
            ImGui::TableSetColumnIndex(0);
            char nameBuf[64];
            strncpy_s(nameBuf, action.actionName.c_str(), _TRUNCATE);
            ImGui::SetNextItemWidth(-1);
            if (ImGui::InputText("##name", nameBuf, sizeof(nameBuf))) {
                action.actionName = nameBuf;
                m_dirty = true;
            }

            // Keyboard binding.
            ImGui::TableSetColumnIndex(1);
            ImGui::PushID("action_keyboard");
            if (ImGui::Button(FormatKeyboardLabel(action.scancode), ImVec2(-1, 0))) {
                m_capturingKey = true;
                m_captureTargetAction = i;
                m_captureTargetAxis = -1;
                m_captureField = CaptureField::ActionKeyboard;
                ImGui::OpenPopup("Input Binding");
            }
            ImGui::PopID();

            // Mouse binding.
            ImGui::TableSetColumnIndex(2);
            ImGui::PushID("action_mouse");
            if (ImGui::Button(FormatMouseButtonLabel(action.mouseButton), ImVec2(-1, 0))) {
                m_capturingKey = true;
                m_captureTargetAction = i;
                m_captureTargetAxis = -1;
                m_captureField = CaptureField::ActionMouse;
                ImGui::OpenPopup("Input Binding");
            }
            ImGui::PopID();

            // Gamepad binding.
            ImGui::TableSetColumnIndex(3);
            ImGui::PushID("action_gamepad");
            if (ImGui::Button(FormatGamepadButtonLabel(action.gamepadButton), ImVec2(-1, 0))) {
                m_capturingKey = true;
                m_captureTargetAction = i;
                m_captureTargetAxis = -1;
                m_captureField = CaptureField::ActionGamepad;
                ImGui::OpenPopup("Input Binding");
            }
            ImGui::PopID();

            // Trigger type.
            ImGui::TableSetColumnIndex(4);
            int trigInt = static_cast<int>(action.trigger);
            const char* trigNames[] = { "Pressed", "Released", "Held", "DoubleTap" };
            ImGui::SetNextItemWidth(-1);
            if (ImGui::Combo("##trig", &trigInt, trigNames, 4)) {
                action.trigger = static_cast<ActionTriggerType>(trigInt);
                m_dirty = true;
            }

            // Remove this binding.
            ImGui::TableSetColumnIndex(5);
            if (ImGui::Button("X")) {
                m_editingMap.actions.erase(m_editingMap.actions.begin() + i);
                m_dirty = true;
                ImGui::PopID();
                break;
            }

            ImGui::PopID();
        }

        ImGui::EndTable();
    }

    if (ImGui::Button("+ Add Action")) {
        ActionBinding ab;
        ab.actionName = "NewAction";
        m_editingMap.actions.push_back(ab);
        m_dirty = true;
    }
}

void InputMappingTab::DrawAxisTable()
{
    if (ImGui::BeginTable("AxesTable", 7,
        ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable))
    {
        ImGui::TableSetupColumn("Axis", ImGuiTableColumnFlags_WidthFixed, 100);
        ImGui::TableSetupColumn("+Key", ImGuiTableColumnFlags_WidthFixed, 90);
        ImGui::TableSetupColumn("-Key", ImGuiTableColumnFlags_WidthFixed, 90);
        ImGui::TableSetupColumn("Gamepad Axis", ImGuiTableColumnFlags_WidthFixed, 130);
        ImGui::TableSetupColumn("DeadZone", ImGuiTableColumnFlags_WidthFixed, 60);
        ImGui::TableSetupColumn("Sens", ImGuiTableColumnFlags_WidthFixed, 60);
        ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthFixed, 30);
        ImGui::TableHeadersRow();

        for (int i = 0; i < (int)m_editingMap.axes.size(); ++i) {
            auto& axis = m_editingMap.axes[i];
            ImGui::PushID(1000 + i);
            ImGui::TableNextRow();

            ImGui::TableSetColumnIndex(0);
            char nameBuf[64];
            strncpy_s(nameBuf, axis.axisName.c_str(), _TRUNCATE);
            ImGui::SetNextItemWidth(-1);
            if (ImGui::InputText("##name", nameBuf, sizeof(nameBuf))) {
                axis.axisName = nameBuf;
                m_dirty = true;
            }

            ImGui::TableSetColumnIndex(1);
            ImGui::PushID("axis_positive_key");
            if (ImGui::Button(FormatKeyboardLabel(axis.positiveKey), ImVec2(-1, 0))) {
                m_capturingKey = true;
                m_captureTargetAction = -1;
                m_captureTargetAxis = i;
                m_captureField = CaptureField::AxisPositiveKey;
                ImGui::OpenPopup("Input Binding");
            }
            ImGui::PopID();

            ImGui::TableSetColumnIndex(2);
            ImGui::PushID("axis_negative_key");
            if (ImGui::Button(FormatKeyboardLabel(axis.negativeKey), ImVec2(-1, 0))) {
                m_capturingKey = true;
                m_captureTargetAction = -1;
                m_captureTargetAxis = i;
                m_captureField = CaptureField::AxisNegativeKey;
                ImGui::OpenPopup("Input Binding");
            }
            ImGui::PopID();

            ImGui::TableSetColumnIndex(3);
            ImGui::PushID("axis_gamepad");
            if (ImGui::Button(FormatGamepadAxisLabel(axis.gamepadAxis), ImVec2(-1, 0))) {
                m_capturingKey = true;
                m_captureTargetAction = -1;
                m_captureTargetAxis = i;
                m_captureField = CaptureField::AxisGamepad;
                ImGui::OpenPopup("Input Binding");
            }
            ImGui::PopID();

            ImGui::TableSetColumnIndex(4);
            ImGui::SetNextItemWidth(-1);
            if (ImGui::DragFloat("##dz", &axis.deadzone, 0.01f, 0.0f, 1.0f)) m_dirty = true;

            ImGui::TableSetColumnIndex(5);
            ImGui::SetNextItemWidth(-1);
            if (ImGui::DragFloat("##sens", &axis.sensitivity, 0.01f, 0.0f, 10.0f)) m_dirty = true;

            ImGui::TableSetColumnIndex(6);
            if (ImGui::Button("X")) {
                m_editingMap.axes.erase(m_editingMap.axes.begin() + i);
                m_dirty = true;
                ImGui::PopID();
                break;
            }

            ImGui::PopID();
        }

        ImGui::EndTable();
    }

    if (ImGui::Button("+ Add Axis")) {
        AxisBinding ab;
        ab.axisName = "NewAxis";
        m_editingMap.axes.push_back(ab);
        m_dirty = true;
    }
}

void InputMappingTab::DrawSettings()
{
    if (ImGui::DragInt("Hold Threshold (frames)", &m_editingMap.holdThresholdFrames, 1, 1, 120))
        m_dirty = true;
    if (ImGui::DragInt("Double Tap Gap (frames)", &m_editingMap.doubleTapGapFrames, 1, 1, 60))
        m_dirty = true;
}

void InputMappingTab::DrawLiveTest(Registry* registry)
{
    if (!registry) {
        ImGui::Text("No registry available.");
        return;
    }

    ImGui::Text("Real-time input state from ResolvedInputStateComponent:");
    ImGui::Separator();

    Signature sig = CreateSignature<ResolvedInputStateComponent, InputActionMapComponent>();
    const ResolvedInputStateComponent* resolved = nullptr;
    const InputActionMapComponent* actionMap = nullptr;

    for (auto* arch : registry->GetAllArchetypes()) {
        if (!SignatureMatches(arch->GetSignature(), sig)) {
            continue;
        }

        auto* resolvedCol = arch->GetColumn(TypeManager::GetComponentTypeID<ResolvedInputStateComponent>());
        auto* actionMapCol = arch->GetColumn(TypeManager::GetComponentTypeID<InputActionMapComponent>());
        if (!resolvedCol || !actionMapCol) {
            continue;
        }

        for (size_t i = 0; i < arch->GetEntityCount(); ++i) {
            auto* candidateMap = static_cast<InputActionMapComponent*>(actionMapCol->Get(i));
            if (!candidateMap) {
                continue;
            }

            if (!m_editingMap.name.empty() && !candidateMap->asset.name.empty() && candidateMap->asset.name != m_editingMap.name) {
                continue;
            }

            resolved = static_cast<ResolvedInputStateComponent*>(resolvedCol->Get(i));
            actionMap = candidateMap;
            break;
        }

        if (resolved) {
            break;
        }
    }

    if (!resolved) {
        ImGui::TextDisabled("No live input source matched the current action map.");
        return;
    }

    if (actionMap && !actionMap->asset.name.empty()) {
        ImGui::TextDisabled("Map: %s", actionMap->asset.name.c_str());
        ImGui::Separator();
    }

    ImGui::Text("Actions:");
    const int actionCount = (std::min)(static_cast<int>(m_editingMap.actions.size()), static_cast<int>(resolved->actionCount));
    for (int i = 0; i < actionCount; ++i) {
        const auto& action = m_editingMap.actions[i];
        const auto& state = resolved->actions[i];
        ImGui::BulletText(
            "%s: P=%d H=%d R=%d V=%.2f",
            action.actionName.c_str(),
            state.pressed ? 1 : 0,
            state.held ? 1 : 0,
            state.released ? 1 : 0,
            state.value);
    }

    ImGui::Separator();
    ImGui::Text("Axes:");
    const int axisCount = (std::min)(static_cast<int>(m_editingMap.axes.size()), static_cast<int>(resolved->axisCount));
    for (int i = 0; i < axisCount; ++i) {
        const auto& axis = m_editingMap.axes[i];
        ImGui::BulletText("%s: %.2f", axis.axisName.c_str(), resolved->axes[i]);
    }
}

void InputMappingTab::DrawKeyBindPopup()
{
    if (ImGui::BeginPopupModal("Input Binding", nullptr,
        ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove))
    {
        ImGui::TextUnformatted(GetCapturePrompt(m_captureField));
        ImGui::TextDisabled("Esc to cancel");

        auto finishCapture = [&]() {
            m_capturingKey = false;
            m_captureTargetAction = -1;
            m_captureTargetAxis = -1;
            ImGui::CloseCurrentPopup();
        };

        if (ImGui::Button("Clear Binding")) {
            switch (m_captureField) {
            case CaptureField::ActionKeyboard:
                if (m_captureTargetAction >= 0 && m_captureTargetAction < static_cast<int>(m_editingMap.actions.size())) {
                    m_editingMap.actions[m_captureTargetAction].scancode = 0;
                    m_dirty = true;
                }
                break;
            case CaptureField::ActionMouse:
                if (m_captureTargetAction >= 0 && m_captureTargetAction < static_cast<int>(m_editingMap.actions.size())) {
                    m_editingMap.actions[m_captureTargetAction].mouseButton = 0;
                    m_dirty = true;
                }
                break;
            case CaptureField::ActionGamepad:
                if (m_captureTargetAction >= 0 && m_captureTargetAction < static_cast<int>(m_editingMap.actions.size())) {
                    m_editingMap.actions[m_captureTargetAction].gamepadButton = 0xFF;
                    m_dirty = true;
                }
                break;
            case CaptureField::AxisPositiveKey:
                if (m_captureTargetAxis >= 0 && m_captureTargetAxis < static_cast<int>(m_editingMap.axes.size())) {
                    m_editingMap.axes[m_captureTargetAxis].positiveKey = 0;
                    m_dirty = true;
                }
                break;
            case CaptureField::AxisNegativeKey:
                if (m_captureTargetAxis >= 0 && m_captureTargetAxis < static_cast<int>(m_editingMap.axes.size())) {
                    m_editingMap.axes[m_captureTargetAxis].negativeKey = 0;
                    m_dirty = true;
                }
                break;
            case CaptureField::AxisGamepad:
                if (m_captureTargetAxis >= 0 && m_captureTargetAxis < static_cast<int>(m_editingMap.axes.size())) {
                    m_editingMap.axes[m_captureTargetAxis].gamepadAxis = 0xFF;
                    m_dirty = true;
                }
                break;
            }
            finishCapture();
            ImGui::EndPopup();
            return;
        }

        if (ImGui::IsKeyPressed(ImGuiKey_Escape)) {
            finishCapture();
            ImGui::EndPopup();
            return;
        }

        if (m_captureField == CaptureField::ActionMouse) {
            uint8_t mouseButton = 0;
            if (TryCaptureMouseButton(mouseButton) &&
                m_captureTargetAction >= 0 &&
                m_captureTargetAction < static_cast<int>(m_editingMap.actions.size()))
            {
                m_editingMap.actions[m_captureTargetAction].mouseButton = mouseButton;
                m_dirty = true;
                finishCapture();
                ImGui::EndPopup();
                return;
            }
        }

        if (m_captureField == CaptureField::ActionGamepad) {
            uint8_t gamepadButton = 0xFF;
            if (TryCaptureGamepadButton(gamepadButton) &&
                m_captureTargetAction >= 0 &&
                m_captureTargetAction < static_cast<int>(m_editingMap.actions.size()))
            {
                m_editingMap.actions[m_captureTargetAction].gamepadButton = gamepadButton;
                m_dirty = true;
                finishCapture();
                ImGui::EndPopup();
                return;
            }
        }

        if (m_captureField == CaptureField::AxisGamepad) {
            uint8_t gamepadAxis = 0xFF;
            if (TryCaptureGamepadAxis(gamepadAxis) &&
                m_captureTargetAxis >= 0 &&
                m_captureTargetAxis < static_cast<int>(m_editingMap.axes.size()))
            {
                m_editingMap.axes[m_captureTargetAxis].gamepadAxis = gamepadAxis;
                m_dirty = true;
                finishCapture();
                ImGui::EndPopup();
                return;
            }
        }

        for (int k = ImGuiKey_NamedKey_BEGIN; k < ImGuiKey_NamedKey_END; ++k) {
            if (ImGui::IsKeyPressed((ImGuiKey)k)) {
                uint32_t scancode = 0;
                if (!TryMapImGuiKeyToSdlScancode(static_cast<ImGuiKey>(k), scancode)) {
                    break;
                }
                if (m_captureField == CaptureField::ActionKeyboard &&
                    m_captureTargetAction >= 0 &&
                    m_captureTargetAction < static_cast<int>(m_editingMap.actions.size()))
                {
                    m_editingMap.actions[m_captureTargetAction].scancode = scancode;
                    m_dirty = true;
                    finishCapture();
                }
                else if (m_captureField == CaptureField::AxisPositiveKey &&
                    m_captureTargetAxis >= 0 &&
                    m_captureTargetAxis < static_cast<int>(m_editingMap.axes.size()))
                {
                    m_editingMap.axes[m_captureTargetAxis].positiveKey = scancode;
                    m_dirty = true;
                    finishCapture();
                }
                else if (m_captureField == CaptureField::AxisNegativeKey &&
                    m_captureTargetAxis >= 0 &&
                    m_captureTargetAxis < static_cast<int>(m_editingMap.axes.size()))
                {
                    m_editingMap.axes[m_captureTargetAxis].negativeKey = scancode;
                    m_dirty = true;
                    finishCapture();
                }
                break;
            }
        }
        ImGui::EndPopup();
    }
}
