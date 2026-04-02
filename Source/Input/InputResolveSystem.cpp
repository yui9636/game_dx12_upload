#include "InputResolveSystem.h"
#include "InputUserComponent.h"
#include "InputBindingComponent.h"
#include "InputContextComponent.h"
#include "ResolvedInputStateComponent.h"
#include "InputActionMapAsset.h"
#include "InputEventQueue.h"
#include "Registry/Registry.h"
#include "Component/ComponentSignature.h"
#include "Type/TypeInfo.h"
#include "Archetype/Archetype.h"
#include <cmath>
#include <cstring>

namespace {
    float ApplyDeadzone(float value, float deadzone) {
        if (std::abs(value) < deadzone) return 0.0f;
        float sign = (value > 0.0f) ? 1.0f : -1.0f;
        return sign * (std::abs(value) - deadzone) / (1.0f - deadzone);
    }
}

void InputResolveSystem::Update(Registry& registry, const InputEventQueue& queue, float dt) {
    Signature sig = CreateSignature<InputUserComponent, InputBindingComponent,
                                     InputContextComponent, ResolvedInputStateComponent>();
    auto archetypes = registry.GetAllArchetypes();

    for (auto* arch : archetypes) {
        if (!SignatureMatches(arch->GetSignature(), sig)) continue;

        auto* userCol = arch->GetColumn(TypeManager::GetComponentTypeID<InputUserComponent>());
        auto* bindCol = arch->GetColumn(TypeManager::GetComponentTypeID<InputBindingComponent>());
        auto* ctxCol  = arch->GetColumn(TypeManager::GetComponentTypeID<InputContextComponent>());
        auto* stateCol = arch->GetColumn(TypeManager::GetComponentTypeID<ResolvedInputStateComponent>());
        if (!userCol || !bindCol || !ctxCol || !stateCol) continue;

        for (size_t i = 0; i < arch->GetEntityCount(); ++i) {
            auto& user  = *static_cast<InputUserComponent*>(userCol->Get(i));
            auto& bind  = *static_cast<InputBindingComponent*>(bindCol->Get(i));
            auto& ctx   = *static_cast<InputContextComponent*>(ctxCol->Get(i));
            auto& state = *static_cast<ResolvedInputStateComponent*>(stateCol->Get(i));

            // Skip consumed contexts
            if (ctx.consumed || !ctx.enabled) {
                // Reset transient states
                for (int a = 0; a < state.actionCount; ++a) {
                    state.actions[a].pressed = false;
                    state.actions[a].released = false;
                    state.actions[a].framesSincePressed++;
                    state.actions[a].framesSinceReleased++;
                }
                state.deltaX = 0.0f;
                state.deltaY = 0.0f;
                state.scrollX = 0.0f;
                state.scrollY = 0.0f;
                continue;
            }

            // Load action map
            InputActionMapAsset* actionMap = nullptr;
            if (bind.actionMapAssetPath[0] != '\0') {
                actionMap = InputActionMapAsset::Get(bind.actionMapAssetPath);
            }

            // Ensure action/axis counts match
            if (actionMap) {
                state.actionCount = static_cast<uint8_t>(
                    (actionMap->actions.size() < ResolvedInputStateComponent::MAX_ACTIONS)
                    ? actionMap->actions.size() : ResolvedInputStateComponent::MAX_ACTIONS);
                state.axisCount = static_cast<uint8_t>(
                    (actionMap->axes.size() < ResolvedInputStateComponent::MAX_AXES)
                    ? actionMap->axes.size() : ResolvedInputStateComponent::MAX_AXES);
            }

            // Save previous held states for edge detection
            bool prevHeld[ResolvedInputStateComponent::MAX_ACTIONS] = {};
            for (int a = 0; a < state.actionCount; ++a) {
                prevHeld[a] = state.actions[a].held;
            }

            // Track raw key states for this frame (accumulate from events)
            bool keyDownThisFrame[ResolvedInputStateComponent::MAX_ACTIONS] = {};
            bool keyUpThisFrame[ResolvedInputStateComponent::MAX_ACTIONS] = {};

            // Reset per-frame mouse deltas
            state.deltaX = 0.0f;
            state.deltaY = 0.0f;
            state.scrollX = 0.0f;
            state.scrollY = 0.0f;
            state.textLength = 0;
            state.textBuffer[0] = '\0';

            // Reset gamepad axes (will be accumulated)
            float gamepadAxes[ResolvedInputStateComponent::MAX_AXES] = {};

            // Process events
            for (auto& ev : queue.GetEvents()) {
                switch (ev.type) {
                case InputEventType::KeyDown:
                    state.lastDeviceType = InputDeviceType::Keyboard;
                    if (actionMap) {
                        for (int a = 0; a < state.actionCount; ++a) {
                            if (actionMap->actions[a].scancode == ev.key.scancode) {
                                keyDownThisFrame[a] = true;
                            }
                        }
                        // Keyboard axis contribution
                        for (int ax = 0; ax < state.axisCount; ++ax) {
                            if (actionMap->axes[ax].positiveKey == ev.key.scancode)
                                gamepadAxes[ax] += 1.0f;
                            if (actionMap->axes[ax].negativeKey == ev.key.scancode)
                                gamepadAxes[ax] -= 1.0f;
                        }
                    }
                    break;

                case InputEventType::KeyUp:
                    if (actionMap) {
                        for (int a = 0; a < state.actionCount; ++a) {
                            if (actionMap->actions[a].scancode == ev.key.scancode) {
                                keyUpThisFrame[a] = true;
                            }
                        }
                    }
                    break;

                case InputEventType::MouseMove:
                    state.pointerX = ev.mouseMove.x;
                    state.pointerY = ev.mouseMove.y;
                    state.deltaX += ev.mouseMove.dx;
                    state.deltaY += ev.mouseMove.dy;
                    state.lastDeviceType = InputDeviceType::Mouse;
                    break;

                case InputEventType::MouseButtonDown:
                    state.lastDeviceType = InputDeviceType::Mouse;
                    if (actionMap) {
                        for (int a = 0; a < state.actionCount; ++a) {
                            if (actionMap->actions[a].mouseButton == ev.mouseButton.button &&
                                actionMap->actions[a].mouseButton != 0) {
                                keyDownThisFrame[a] = true;
                            }
                        }
                    }
                    break;

                case InputEventType::MouseButtonUp:
                    if (actionMap) {
                        for (int a = 0; a < state.actionCount; ++a) {
                            if (actionMap->actions[a].mouseButton == ev.mouseButton.button &&
                                actionMap->actions[a].mouseButton != 0) {
                                keyUpThisFrame[a] = true;
                            }
                        }
                    }
                    break;

                case InputEventType::MouseWheel:
                    state.scrollX += ev.mouseWheel.scrollX;
                    state.scrollY += ev.mouseWheel.scrollY;
                    break;

                case InputEventType::GamepadButtonDown:
                    state.lastDeviceType = InputDeviceType::Gamepad;
                    if (actionMap) {
                        for (int a = 0; a < state.actionCount; ++a) {
                            if (actionMap->actions[a].gamepadButton == ev.gamepadButton.button) {
                                keyDownThisFrame[a] = true;
                            }
                        }
                    }
                    break;

                case InputEventType::GamepadButtonUp:
                    if (actionMap) {
                        for (int a = 0; a < state.actionCount; ++a) {
                            if (actionMap->actions[a].gamepadButton == ev.gamepadButton.button) {
                                keyUpThisFrame[a] = true;
                            }
                        }
                    }
                    break;

                case InputEventType::GamepadAxis:
                    state.lastDeviceType = InputDeviceType::Gamepad;
                    if (actionMap) {
                        for (int ax = 0; ax < state.axisCount; ++ax) {
                            if (actionMap->axes[ax].gamepadAxis == ev.gamepadAxis.axis) {
                                gamepadAxes[ax] = ev.gamepadAxis.value;
                            }
                        }
                    }
                    break;

                case InputEventType::TextInput: {
                    size_t len = strlen(ev.textInput.text);
                    if (state.textLength + len < sizeof(state.textBuffer) - 1) {
                        memcpy(state.textBuffer + state.textLength, ev.textInput.text, len);
                        state.textLength += static_cast<uint16_t>(len);
                        state.textBuffer[state.textLength] = '\0';
                    }
                    break;
                }

                default:
                    break;
                }
            }

            // Update action edge states
            for (int a = 0; a < state.actionCount; ++a) {
                auto& action = state.actions[a];
                bool wasHeld = prevHeld[a];

                if (keyDownThisFrame[a]) {
                    action.held = true;
                }
                if (keyUpThisFrame[a]) {
                    action.held = false;
                }

                action.pressed = action.held && !wasHeld;
                action.released = !action.held && wasHeld;
                action.value = action.held ? 1.0f : 0.0f;

                if (action.pressed) {
                    action.framesSincePressed = 0;
                } else {
                    action.framesSincePressed++;
                }

                if (action.released) {
                    action.framesSinceReleased = 0;
                } else {
                    action.framesSinceReleased++;
                }
            }

            // Update axes with deadzone
            for (int ax = 0; ax < state.axisCount; ++ax) {
                float deadzone = actionMap ? actionMap->axes[ax].deadzone : 0.15f;
                float sens = actionMap ? actionMap->axes[ax].sensitivity : 1.0f;
                state.axes[ax] = ApplyDeadzone(gamepadAxes[ax], deadzone) * sens;
                // Clamp
                if (state.axes[ax] > 1.0f) state.axes[ax] = 1.0f;
                if (state.axes[ax] < -1.0f) state.axes[ax] = -1.0f;
            }
        }
    }
}
