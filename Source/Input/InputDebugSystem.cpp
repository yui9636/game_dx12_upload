#include "InputDebugSystem.h"
#include "InputDebugStateComponent.h"
#include "InputContextComponent.h"
#include "InputUserComponent.h"
#include "InputBindingComponent.h"
#include "ResolvedInputStateComponent.h"
#include "InputActionMapAsset.h"
#include "InputBindingProfileAsset.h"
#include "InputEventQueue.h"
#include "IInputBackend.h"
#include "Registry/Registry.h"
#include "Component/ComponentSignature.h"
#include "Type/TypeInfo.h"
#include "Archetype/Archetype.h"
#include <imgui.h>
#include <cstring>
#include <string>

// ---- Rebind state (file-local) ----
static bool s_rebindActive = false;
static std::string s_rebindMapPath;
static int s_rebindActionIndex = -1;
static std::string s_rebindActionName;

void InputDebugSystem::Update(Registry& registry, const InputEventQueue& queue) {
    Signature sig = CreateSignature<InputDebugStateComponent>();
    auto archetypes = registry.GetAllArchetypes();

    for (auto* arch : archetypes) {
        if (!SignatureMatches(arch->GetSignature(), sig)) continue;
        auto* col = arch->GetColumn(TypeManager::GetComponentTypeID<InputDebugStateComponent>());
        if (!col) continue;

        for (size_t i = 0; i < arch->GetEntityCount(); ++i) {
            auto& debug = *static_cast<InputDebugStateComponent*>(col->Get(i));
            for (auto& ev : queue.GetEvents()) {
                debug.PushEvent(ev);
            }
        }
    }
}

static const char* EventTypeName(InputEventType type) {
    switch (type) {
    case InputEventType::KeyDown: return "KeyDown";
    case InputEventType::KeyUp: return "KeyUp";
    case InputEventType::MouseMove: return "MouseMove";
    case InputEventType::MouseButtonDown: return "MouseBtnDown";
    case InputEventType::MouseButtonUp: return "MouseBtnUp";
    case InputEventType::MouseWheel: return "MouseWheel";
    case InputEventType::GamepadButtonDown: return "PadBtnDown";
    case InputEventType::GamepadButtonUp: return "PadBtnUp";
    case InputEventType::GamepadAxis: return "PadAxis";
    case InputEventType::TextInput: return "TextInput";
    case InputEventType::TextComposition: return "TextComp";
    case InputEventType::DeviceAdded: return "DeviceAdded";
    case InputEventType::DeviceRemoved: return "DeviceRemoved";
    case InputEventType::WindowFocusGained: return "FocusGained";
    case InputEventType::WindowFocusLost: return "FocusLost";
    default: return "Unknown";
    }
}

static const char* GetScancodeLabel(uint32_t scancode) {
    // Common SDL scancodes
    static char buf[16];
    switch (scancode) {
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
    case 40: return "Enter"; case 41: return "Escape"; case 42: return "Backspace";
    case 43: return "Tab"; case 44: return "Space";
    case 58: return "F1"; case 59: return "F2"; case 60: return "F3"; case 61: return "F4";
    case 62: return "F5"; case 63: return "F6"; case 64: return "F7"; case 65: return "F8";
    case 76: return "Delete"; case 225: return "LShift"; case 224: return "LCtrl";
    case 226: return "LAlt";
    default:
        snprintf(buf, sizeof(buf), "SC:%u", scancode);
        return buf;
    }
}

static const char* GetGamepadButtonLabel(uint8_t button) {
    switch (button) {
    case 0: return "A"; case 1: return "B"; case 2: return "X"; case 3: return "Y";
    case 4: return "Back"; case 5: return "Guide"; case 6: return "Start";
    case 7: return "LS"; case 8: return "RS"; case 9: return "LB"; case 10: return "RB";
    case 11: return "DPad Up"; case 12: return "DPad Down";
    case 13: return "DPad Left"; case 14: return "DPad Right";
    case 0xFF: return "-";
    default: return "?";
    }
}

static const char* GetGamepadAxisLabel(uint8_t axis) {
    switch (axis) {
    case 0: return "LStickX"; case 1: return "LStickY";
    case 2: return "RStickX"; case 3: return "RStickY";
    case 4: return "LTrigger"; case 5: return "RTrigger";
    case 0xFF: return "-";
    default: return "?";
    }
}

static const char* GetMouseButtonLabel(uint8_t btn) {
    switch (btn) {
    case 0: return "LMB"; case 1: return "MMB"; case 2: return "RMB";
    default: return "-";
    }
}

static void DrawRebindPopup(const InputEventQueue& queue) {
    if (!s_rebindActive) return;

    ImGui::OpenPopup("Rebind Input");
    if (ImGui::BeginPopupModal("Rebind Input", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("Rebind: \"%s\"", s_rebindActionName.c_str());
        ImGui::Separator();
        ImGui::Text("Press any key, mouse button, or gamepad button...");
        ImGui::Spacing();

        bool captured = false;
        for (auto& ev : queue.GetEvents()) {
            InputActionMapAsset* map = InputActionMapAsset::Get(s_rebindMapPath);
            if (!map || s_rebindActionIndex < 0 || s_rebindActionIndex >= (int)map->actions.size()) break;
            auto& binding = map->actions[s_rebindActionIndex];

            if (ev.type == InputEventType::KeyDown) {
                if (ev.key.scancode == 41) { // Escape = cancel
                    captured = true;
                    break;
                }
                binding.scancode = ev.key.scancode;
                captured = true;
            } else if (ev.type == InputEventType::GamepadButtonDown) {
                binding.gamepadButton = ev.gamepadButton.button;
                captured = true;
            } else if (ev.type == InputEventType::MouseButtonDown) {
                binding.mouseButton = ev.mouseButton.button;
                captured = true;
            }
        }

        if (captured) {
            // Save to profile
            InputActionMapAsset* map = InputActionMapAsset::Get(s_rebindMapPath);
            if (map) {
                map->SaveToFile(s_rebindMapPath);
            }
            s_rebindActive = false;
            ImGui::CloseCurrentPopup();
        }

        if (ImGui::Button("Cancel")) {
            s_rebindActive = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Clear")) {
            InputActionMapAsset* map = InputActionMapAsset::Get(s_rebindMapPath);
            if (map && s_rebindActionIndex >= 0 && s_rebindActionIndex < (int)map->actions.size()) {
                auto& binding = map->actions[s_rebindActionIndex];
                binding.scancode = 0;
                binding.mouseButton = 0;
                binding.gamepadButton = 0xFF;
                map->SaveToFile(s_rebindMapPath);
            }
            s_rebindActive = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
}

static void DrawBindingsTab(Registry& registry) {
    // Collect all binding components to find which action maps are in use
    Signature sig = CreateSignature<InputBindingComponent>();
    auto archetypes = registry.GetAllArchetypes();

    // Gather unique map paths
    std::vector<std::string> mapPaths;
    for (auto* arch : archetypes) {
        if (!SignatureMatches(arch->GetSignature(), sig)) continue;
        auto* col = arch->GetColumn(TypeManager::GetComponentTypeID<InputBindingComponent>());
        if (!col) continue;
        for (size_t i = 0; i < arch->GetEntityCount(); ++i) {
            auto& bind = *static_cast<InputBindingComponent*>(col->Get(i));
            if (bind.actionMapAssetPath[0] != '\0') {
                std::string path(bind.actionMapAssetPath);
                bool found = false;
                for (auto& p : mapPaths) { if (p == path) { found = true; break; } }
                if (!found) mapPaths.push_back(path);
            }
        }
    }

    // Also try common paths
    const char* commonMaps[] = {
        "Data/Input/EditorGlobal.inputmap",
        "Data/Input/SceneView.inputmap",
        "Data/Input/Gameplay.inputmap"
    };
    for (auto* path : commonMaps) {
        std::string p(path);
        bool found = false;
        for (auto& mp : mapPaths) { if (mp == p) { found = true; break; } }
        if (!found) mapPaths.push_back(p);
    }

    for (auto& mapPath : mapPaths) {
        InputActionMapAsset* map = InputActionMapAsset::Get(mapPath);
        if (!map) continue;

        if (ImGui::TreeNodeEx(map->name.c_str(), ImGuiTreeNodeFlags_DefaultOpen)) {
            // Actions table
            if (!map->actions.empty()) {
                ImGui::Text("Actions:");
                ImGui::Columns(4, "BindCols");
                ImGui::SetColumnWidth(0, 140);
                ImGui::SetColumnWidth(1, 100);
                ImGui::SetColumnWidth(2, 80);
                ImGui::SetColumnWidth(3, 100);
                ImGui::Text("Action"); ImGui::NextColumn();
                ImGui::Text("Keyboard"); ImGui::NextColumn();
                ImGui::Text("Mouse"); ImGui::NextColumn();
                ImGui::Text("Gamepad"); ImGui::NextColumn();
                ImGui::Separator();

                for (int ai = 0; ai < (int)map->actions.size(); ++ai) {
                    auto& act = map->actions[ai];
                    ImGui::Text("%s", act.actionName.c_str()); ImGui::NextColumn();

                    // Keyboard binding - clickable
                    ImGui::PushID(ai * 3 + 0);
                    if (ImGui::SmallButton(act.scancode ? GetScancodeLabel(act.scancode) : "-")) {
                        s_rebindActive = true;
                        s_rebindMapPath = mapPath;
                        s_rebindActionIndex = ai;
                        s_rebindActionName = act.actionName;
                    }
                    ImGui::PopID();
                    ImGui::NextColumn();

                    // Mouse binding
                    ImGui::PushID(ai * 3 + 1);
                    if (ImGui::SmallButton(GetMouseButtonLabel(act.mouseButton))) {
                        s_rebindActive = true;
                        s_rebindMapPath = mapPath;
                        s_rebindActionIndex = ai;
                        s_rebindActionName = act.actionName;
                    }
                    ImGui::PopID();
                    ImGui::NextColumn();

                    // Gamepad binding
                    ImGui::PushID(ai * 3 + 2);
                    if (ImGui::SmallButton(GetGamepadButtonLabel(act.gamepadButton))) {
                        s_rebindActive = true;
                        s_rebindMapPath = mapPath;
                        s_rebindActionIndex = ai;
                        s_rebindActionName = act.actionName;
                    }
                    ImGui::PopID();
                    ImGui::NextColumn();
                }
                ImGui::Columns(1);
            }

            // Axes table
            if (!map->axes.empty()) {
                ImGui::Spacing();
                ImGui::Text("Axes:");
                ImGui::Columns(4, "AxisCols");
                ImGui::SetColumnWidth(0, 140);
                ImGui::SetColumnWidth(1, 100);
                ImGui::SetColumnWidth(2, 80);
                ImGui::SetColumnWidth(3, 100);
                ImGui::Text("Axis"); ImGui::NextColumn();
                ImGui::Text("+/-Key"); ImGui::NextColumn();
                ImGui::Text("Deadzone"); ImGui::NextColumn();
                ImGui::Text("Gamepad"); ImGui::NextColumn();
                ImGui::Separator();

                for (auto& ax : map->axes) {
                    ImGui::Text("%s", ax.axisName.c_str()); ImGui::NextColumn();
                    if (ax.positiveKey || ax.negativeKey) {
                        ImGui::Text("%s/%s", GetScancodeLabel(ax.positiveKey), GetScancodeLabel(ax.negativeKey));
                    } else {
                        ImGui::Text("-");
                    }
                    ImGui::NextColumn();
                    ImGui::Text("%.2f", ax.deadzone); ImGui::NextColumn();
                    ImGui::Text("%s", GetGamepadAxisLabel(ax.gamepadAxis)); ImGui::NextColumn();
                }
                ImGui::Columns(1);
            }

            ImGui::TreePop();
        }
    }
}

void InputDebugSystem::DrawDebugWindow(Registry& registry, IInputBackend& backend, const InputEventQueue& queue) {
    if (!ImGui::Begin("Input Debug")) {
        ImGui::End();
        return;
    }

    if (ImGui::BeginTabBar("InputDebugTabs")) {
        // Tab: Connected Devices
        if (ImGui::BeginTabItem("Devices")) {
            auto devices = backend.GetConnectedDevices();
            ImGui::Columns(3);
            ImGui::Text("ID"); ImGui::NextColumn();
            ImGui::Text("Type"); ImGui::NextColumn();
            ImGui::Text("Name"); ImGui::NextColumn();
            ImGui::Separator();
            for (auto& d : devices) {
                ImGui::Text("%u", d.deviceId); ImGui::NextColumn();
                const char* typeName = "Unknown";
                if (d.type == InputDeviceType::Keyboard) typeName = "Keyboard";
                else if (d.type == InputDeviceType::Mouse) typeName = "Mouse";
                else if (d.type == InputDeviceType::Gamepad) typeName = "Gamepad";
                ImGui::Text("%s", typeName); ImGui::NextColumn();
                ImGui::Text("%s", d.name); ImGui::NextColumn();
            }
            ImGui::Columns(1);
            ImGui::EndTabItem();
        }

        // Tab: Active Contexts
        if (ImGui::BeginTabItem("Contexts")) {
            Signature sig = CreateSignature<InputContextComponent>();
            auto archetypes = registry.GetAllArchetypes();
            for (auto* arch : archetypes) {
                if (!SignatureMatches(arch->GetSignature(), sig)) continue;
                auto* col = arch->GetColumn(TypeManager::GetComponentTypeID<InputContextComponent>());
                if (!col) continue;
                for (size_t i = 0; i < arch->GetEntityCount(); ++i) {
                    auto& ctx = *static_cast<InputContextComponent*>(col->Get(i));
                    const char* priorityNames[] = {
                        "EditorGlobal", "SceneView", "RuntimeGameplay", "RuntimeUI",
                        "Console", "TextInput", "ModalDialog"
                    };
                    int pi = (int)ctx.priority;
                    const char* pname = (pi >= 0 && pi < 7) ? priorityNames[pi] : "?";
                    ImGui::Text("[%s] Enabled: %s | Consumed: %s | ConsumeLower: %s",
                        pname,
                        ctx.enabled ? "Y" : "N",
                        ctx.consumed ? "Y" : "N",
                        ctx.consumeLowerPriority ? "Y" : "N");
                }
            }
            ImGui::EndTabItem();
        }

        // Tab: Resolved Actions
        if (ImGui::BeginTabItem("Actions")) {
            Signature sig = CreateSignature<ResolvedInputStateComponent>();
            auto archetypes = registry.GetAllArchetypes();
            for (auto* arch : archetypes) {
                if (!SignatureMatches(arch->GetSignature(), sig)) continue;
                auto* col = arch->GetColumn(TypeManager::GetComponentTypeID<ResolvedInputStateComponent>());
                if (!col) continue;
                for (size_t i = 0; i < arch->GetEntityCount(); ++i) {
                    auto& state = *static_cast<ResolvedInputStateComponent*>(col->Get(i));
                    ImGui::Text("Actions (%d):", state.actionCount);
                    for (int a = 0; a < state.actionCount; ++a) {
                        auto& act = state.actions[a];
                        ImGui::Text("  [%d] P:%d H:%d R:%d val:%.2f fSP:%d fSR:%d",
                            a, act.pressed, act.held, act.released, act.value,
                            act.framesSincePressed, act.framesSinceReleased);
                    }
                    ImGui::Text("Axes (%d):", state.axisCount);
                    for (int ax = 0; ax < state.axisCount; ++ax) {
                        ImGui::Text("  [%d] %.3f", ax, state.axes[ax]);
                    }
                    ImGui::Text("Pointer: (%.1f, %.1f) Delta: (%.1f, %.1f) Scroll: (%.1f, %.1f)",
                        state.pointerX, state.pointerY, state.deltaX, state.deltaY,
                        state.scrollX, state.scrollY);
                }
            }
            ImGui::EndTabItem();
        }

        // Tab: Bindings (NEW)
        if (ImGui::BeginTabItem("Bindings")) {
            DrawBindingsTab(registry);
            ImGui::EndTabItem();
        }

        // Tab: Event Log
        if (ImGui::BeginTabItem("Event Log")) {
            Signature sig = CreateSignature<InputDebugStateComponent>();
            auto archetypes = registry.GetAllArchetypes();
            for (auto* arch : archetypes) {
                if (!SignatureMatches(arch->GetSignature(), sig)) continue;
                auto* col = arch->GetColumn(TypeManager::GetComponentTypeID<InputDebugStateComponent>());
                if (!col) continue;
                for (size_t i = 0; i < arch->GetEntityCount(); ++i) {
                    auto& debug = *static_cast<InputDebugStateComponent*>(col->Get(i));
                    ImGui::BeginChild("EventLog", ImVec2(0, 300), true);
                    for (uint16_t j = 0; j < debug.historyCount; ++j) {
                        uint16_t idx = (debug.historyHead - debug.historyCount + j + InputDebugStateComponent::HISTORY_SIZE)
                                       % InputDebugStateComponent::HISTORY_SIZE;
                        auto& ev = debug.history[idx];
                        ImGui::Text("[%05u] %s dev:%u",
                            ev.sequence, EventTypeName(ev.type), ev.deviceId);
                    }
                    if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY())
                        ImGui::SetScrollHereY(1.0f);
                    ImGui::EndChild();
                }
            }
            ImGui::EndTabItem();
        }

        ImGui::EndTabBar();
    }

    // Rebind popup (drawn outside tab bar)
    DrawRebindPopup(queue);

    ImGui::End();
}
