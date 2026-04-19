#include "InputMappingTab.h"
#include <imgui.h>
#include "Icon/IconsFontAwesome7.h"
#include "Input/InputBindingComponent.h"
#include "Input/ResolvedInputStateComponent.h"
#include "Archetype/Archetype.h"
#include "Component/ComponentSignature.h"
#include "Registry/Registry.h"
#include "System/Dialog.h"
#include "Type/TypeInfo.h"

namespace
{
    constexpr const char* kInputMapFilter =
        "Input Map (*.inputmap.json)\0*.inputmap.json\0JSON (*.json)\0*.json\0All Files (*.*)\0*.*\0";
}

bool InputMappingTab::OpenActionMap(const std::string& path)
{
    if (path.empty()) {
        return false;
    }

    InputActionMapAsset loaded;
    if (!loaded.LoadFromFile(path)) {
        return false;
    }

    m_actionMapPath = path;
    m_editingMap = std::move(loaded);
    m_dirty = false;
    return true;
}

bool InputMappingTab::SaveActionMap()
{
    if (m_actionMapPath.empty()) {
        return false;
    }

    if (!m_editingMap.SaveToFile(m_actionMapPath)) {
        return false;
    }

    InputActionMapAsset::ClearCache();
    m_dirty = false;
    return true;
}

bool InputMappingTab::SaveActionMapAs(const std::string& path)
{
    if (path.empty()) {
        return false;
    }

    m_actionMapPath = path;
    return SaveActionMap();
}

bool InputMappingTab::ReloadActionMap()
{
    if (m_actionMapPath.empty()) {
        return false;
    }

    return OpenActionMap(m_actionMapPath);
}

void InputMappingTab::SetActionMapPath(const std::string& path)
{
    if (path == m_actionMapPath) {
        return;
    }

    if (path.empty()) {
        m_actionMapPath.clear();
        m_editingMap = InputActionMapAsset{};
        m_dirty = false;
        return;
    }

    OpenActionMap(path);
}

void InputMappingTab::Draw(Registry* registry)
{
    // Action map selector
    if (!m_actionMapPath.empty()) {
        ImGui::Text("Action Map: %s", m_actionMapPath.c_str());
        ImGui::SameLine();
    }
    if (ImGui::Button("Load...")) {
        char pathBuffer[MAX_PATH] = {};
        if (!m_actionMapPath.empty()) {
            strcpy_s(pathBuffer, m_actionMapPath.c_str());
        }
        if (Dialog::OpenFileName(pathBuffer, MAX_PATH, kInputMapFilter, "Open Input Map") == DialogResult::OK) {
            OpenActionMap(pathBuffer);
        }
    }
    if (m_dirty) {
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(1, 0.8f, 0, 1), "(unsaved)");
    }

    ImGui::Separator();

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
        ImGui::TableSetupColumn("Keyboard", ImGuiTableColumnFlags_WidthFixed, 80);
        ImGui::TableSetupColumn("Mouse", ImGuiTableColumnFlags_WidthFixed, 60);
        ImGui::TableSetupColumn("Gamepad", ImGuiTableColumnFlags_WidthFixed, 80);
        ImGui::TableSetupColumn("Trigger", ImGuiTableColumnFlags_WidthFixed, 80);
        ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthFixed, 30);
        ImGui::TableHeadersRow();

        for (int i = 0; i < (int)m_editingMap.actions.size(); ++i) {
            auto& action = m_editingMap.actions[i];
            ImGui::PushID(i);
            ImGui::TableNextRow();

            // Name
            ImGui::TableSetColumnIndex(0);
            char nameBuf[64];
            strncpy_s(nameBuf, action.actionName.c_str(), _TRUNCATE);
            ImGui::SetNextItemWidth(-1);
            if (ImGui::InputText("##name", nameBuf, sizeof(nameBuf))) {
                action.actionName = nameBuf;
                m_dirty = true;
            }

            // Keyboard
            ImGui::TableSetColumnIndex(1);
            char kbLabel[32];
            snprintf(kbLabel, sizeof(kbLabel), "SC:%u", action.scancode);
            if (ImGui::Button(kbLabel, ImVec2(-1, 0))) {
                m_capturingKey = true;
                m_captureTargetAction = i;
                m_captureField = CaptureField::Keyboard;
                ImGui::OpenPopup("KeyCapture");
            }

            // Mouse
            ImGui::TableSetColumnIndex(2);
            char msLabel[16];
            snprintf(msLabel, sizeof(msLabel), "M%u", action.mouseButton);
            if (ImGui::Button(msLabel, ImVec2(-1, 0))) {
                m_capturingKey = true;
                m_captureTargetAction = i;
                m_captureField = CaptureField::Mouse;
                ImGui::OpenPopup("KeyCapture");
            }

            // Gamepad
            ImGui::TableSetColumnIndex(3);
            char gpLabel[16];
            if (action.gamepadButton == 0xFF)
                snprintf(gpLabel, sizeof(gpLabel), "--");
            else
                snprintf(gpLabel, sizeof(gpLabel), "GP%u", action.gamepadButton);
            if (ImGui::Button(gpLabel, ImVec2(-1, 0))) {
                m_capturingKey = true;
                m_captureTargetAction = i;
                m_captureField = CaptureField::Gamepad;
                ImGui::OpenPopup("KeyCapture");
            }

            // Trigger type
            ImGui::TableSetColumnIndex(4);
            int trigInt = static_cast<int>(action.trigger);
            const char* trigNames[] = { "Pressed", "Released", "Held", "DoubleTap" };
            ImGui::SetNextItemWidth(-1);
            if (ImGui::Combo("##trig", &trigInt, trigNames, 4)) {
                action.trigger = static_cast<ActionTriggerType>(trigInt);
                m_dirty = true;
            }

            // Delete
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
        ImGui::TableSetupColumn("+Key", ImGuiTableColumnFlags_WidthFixed, 60);
        ImGui::TableSetupColumn("-Key", ImGuiTableColumnFlags_WidthFixed, 60);
        ImGui::TableSetupColumn("GP Axis", ImGuiTableColumnFlags_WidthFixed, 60);
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
            ImGui::SetNextItemWidth(-1);
            if (ImGui::DragScalar("##pk", ImGuiDataType_U32, &axis.positiveKey, 1.0f)) m_dirty = true;

            ImGui::TableSetColumnIndex(2);
            ImGui::SetNextItemWidth(-1);
            if (ImGui::DragScalar("##nk", ImGuiDataType_U32, &axis.negativeKey, 1.0f)) m_dirty = true;

            ImGui::TableSetColumnIndex(3);
            int gpAxis = axis.gamepadAxis;
            ImGui::SetNextItemWidth(-1);
            if (ImGui::DragInt("##gpa", &gpAxis, 1, 0, 15)) { axis.gamepadAxis = (uint8_t)gpAxis; m_dirty = true; }

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

    Signature sig = CreateSignature<ResolvedInputStateComponent>();
    const ResolvedInputStateComponent* resolved = nullptr;
    const InputBindingComponent* binding = nullptr;

    for (auto* arch : registry->GetAllArchetypes()) {
        if (!SignatureMatches(arch->GetSignature(), sig)) {
            continue;
        }

        auto* resolvedCol = arch->GetColumn(TypeManager::GetComponentTypeID<ResolvedInputStateComponent>());
        auto* bindingCol = arch->GetSignature().test(TypeManager::GetComponentTypeID<InputBindingComponent>())
            ? arch->GetColumn(TypeManager::GetComponentTypeID<InputBindingComponent>())
            : nullptr;
        if (!resolvedCol) {
            continue;
        }

        for (size_t i = 0; i < arch->GetEntityCount(); ++i) {
            auto* candidateBinding = bindingCol ? static_cast<InputBindingComponent*>(bindingCol->Get(i)) : nullptr;
            if (!m_actionMapPath.empty() && candidateBinding && strcmp(candidateBinding->actionMapAssetPath, m_actionMapPath.c_str()) != 0) {
                continue;
            }

            resolved = static_cast<ResolvedInputStateComponent*>(resolvedCol->Get(i));
            binding = candidateBinding;
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

    if (binding && binding->actionMapAssetPath[0] != '\0') {
        ImGui::TextDisabled("Binding: %s", binding->actionMapAssetPath);
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
    if (ImGui::BeginPopupModal("KeyCapture", nullptr,
        ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove))
    {
        ImGui::Text("Press any key/button...");
        ImGui::Text("(ESC to cancel)");

        // Check for key press via ImGui IO
        auto& io = ImGui::GetIO();
        for (int k = ImGuiKey_NamedKey_BEGIN; k < ImGuiKey_NamedKey_END; ++k) {
            if (ImGui::IsKeyPressed((ImGuiKey)k)) {
                if (k == ImGuiKey_Escape) {
                    m_capturingKey = false;
                    ImGui::CloseCurrentPopup();
                }
                else if (m_captureTargetAction >= 0 && m_captureTargetAction < (int)m_editingMap.actions.size()) {
                    // Store scancode (simplified - in real impl, convert ImGuiKey to SDL scancode)
                    m_editingMap.actions[m_captureTargetAction].scancode = (uint32_t)k;
                    m_dirty = true;
                    m_capturingKey = false;
                    ImGui::CloseCurrentPopup();
                }
                break;
            }
        }
        ImGui::EndPopup();
    }
}
