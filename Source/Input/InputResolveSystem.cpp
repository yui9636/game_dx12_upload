#include "InputResolveSystem.h"

#include "Archetype/Archetype.h"
#include "Component/ComponentSignature.h"
#include "InputActionMapAsset.h"
#include "InputActionMapComponent.h"
#include "InputBindingComponent.h"
#include "InputContextComponent.h"
#include "InputEventQueue.h"
#include "InputUserComponent.h"
#include "Registry/Registry.h"
#include "ResolvedInputStateComponent.h"
#include "Type/TypeInfo.h"

#include <cmath>
#include <cstring>

namespace
{
    constexpr uint32_t kMaxTrackedScancodes = 512;
    constexpr uint32_t kMaxTrackedMouseButtons = 16;
    constexpr uint32_t kMaxTrackedGamepadButtons = 32;
    constexpr uint32_t kMaxTrackedGamepadAxes = 16;

    bool g_keyHeld[kMaxTrackedScancodes] = {};
    bool g_mouseHeld[kMaxTrackedMouseButtons] = {};
    bool g_gamepadButtonHeld[kMaxTrackedGamepadButtons] = {};
    float g_gamepadAxisValue[kMaxTrackedGamepadAxes] = {};

    float ApplyDeadzone(float value, float deadzone)
    {
        if (std::abs(value) < deadzone) {
            return 0.0f;
        }
        const float sign = (value > 0.0f) ? 1.0f : -1.0f;
        return sign * (std::abs(value) - deadzone) / (1.0f - deadzone);
    }

    const InputActionMapAsset* ResolveActionMap(const InputActionMapComponent& component)
    {
        if (component.asset.name.empty() &&
            component.asset.contextCategory.empty() &&
            component.asset.actions.empty() &&
            component.asset.axes.empty())
        {
            return nullptr;
        }
        return &component.asset;
    }

    void ClearHeldInputSnapshot()
    {
        std::memset(g_keyHeld, 0, sizeof(g_keyHeld));
        std::memset(g_mouseHeld, 0, sizeof(g_mouseHeld));
        std::memset(g_gamepadButtonHeld, 0, sizeof(g_gamepadButtonHeld));
        std::memset(g_gamepadAxisValue, 0, sizeof(g_gamepadAxisValue));
    }

    void UpdateHeldInputSnapshot(const InputEventQueue& queue)
    {
        for (const auto& ev : queue.GetEvents()) {
            switch (ev.type) {
            case InputEventType::KeyDown:
                if (ev.key.scancode < kMaxTrackedScancodes) {
                    g_keyHeld[ev.key.scancode] = true;
                }
                break;
            case InputEventType::KeyUp:
                if (ev.key.scancode < kMaxTrackedScancodes) {
                    g_keyHeld[ev.key.scancode] = false;
                }
                break;
            case InputEventType::MouseButtonDown:
                if (ev.mouseButton.button < kMaxTrackedMouseButtons) {
                    g_mouseHeld[ev.mouseButton.button] = true;
                }
                break;
            case InputEventType::MouseButtonUp:
                if (ev.mouseButton.button < kMaxTrackedMouseButtons) {
                    g_mouseHeld[ev.mouseButton.button] = false;
                }
                break;
            case InputEventType::GamepadButtonDown:
                if (ev.gamepadButton.button < kMaxTrackedGamepadButtons) {
                    g_gamepadButtonHeld[ev.gamepadButton.button] = true;
                }
                break;
            case InputEventType::GamepadButtonUp:
                if (ev.gamepadButton.button < kMaxTrackedGamepadButtons) {
                    g_gamepadButtonHeld[ev.gamepadButton.button] = false;
                }
                break;
            case InputEventType::GamepadAxis:
                if (ev.gamepadAxis.axis < kMaxTrackedGamepadAxes) {
                    g_gamepadAxisValue[ev.gamepadAxis.axis] = ev.gamepadAxis.value;
                }
                break;
            case InputEventType::WindowFocusLost:
            case InputEventType::DeviceRemoved:
                ClearHeldInputSnapshot();
                break;
            default:
                break;
            }
        }
    }

    bool IsKeyHeld(uint32_t scancode)
    {
        return scancode != 0 && scancode < kMaxTrackedScancodes && g_keyHeld[scancode];
    }

    bool IsActionHeld(const ActionBinding& action)
    {
        if (IsKeyHeld(action.scancode)) {
            return true;
        }
        if (action.mouseButton != 0 &&
            action.mouseButton < kMaxTrackedMouseButtons &&
            g_mouseHeld[action.mouseButton])
        {
            return true;
        }
        if (action.gamepadButton != 0xFF &&
            action.gamepadButton < kMaxTrackedGamepadButtons &&
            g_gamepadButtonHeld[action.gamepadButton])
        {
            return true;
        }
        if (action.gamepadAxis != 0xFF && action.gamepadAxis < kMaxTrackedGamepadAxes) {
            const float value = g_gamepadAxisValue[action.gamepadAxis];
            return action.axisDirection >= 0.0f ? value > 0.5f : value < -0.5f;
        }
        return false;
    }

    float ResolveAxisValue(const AxisBinding& axis)
    {
        float value = 0.0f;
        if (IsKeyHeld(axis.positiveKey)) {
            value += 1.0f;
        }
        if (IsKeyHeld(axis.negativeKey)) {
            value -= 1.0f;
        }
        if (axis.gamepadAxis != 0xFF && axis.gamepadAxis < kMaxTrackedGamepadAxes) {
            const float gamepadValue = g_gamepadAxisValue[axis.gamepadAxis];
            if (std::abs(gamepadValue) > std::abs(value)) {
                value = gamepadValue;
            }
        }
        return value;
    }
}

void InputResolveSystem::Update(Registry& registry, const InputEventQueue& queue, float dt)
{
    UpdateHeldInputSnapshot(queue);

    Signature sig = CreateSignature<
        InputUserComponent,
        InputBindingComponent,
        InputActionMapComponent,
        InputContextComponent,
        ResolvedInputStateComponent>();

    auto archetypes = registry.GetAllArchetypes();

    for (auto* arch : archetypes) {
        if (!SignatureMatches(arch->GetSignature(), sig)) {
            continue;
        }

        auto* userCol = arch->GetColumn(TypeManager::GetComponentTypeID<InputUserComponent>());
        auto* bindCol = arch->GetColumn(TypeManager::GetComponentTypeID<InputBindingComponent>());
        auto* actionMapCol = arch->GetColumn(TypeManager::GetComponentTypeID<InputActionMapComponent>());
        auto* ctxCol = arch->GetColumn(TypeManager::GetComponentTypeID<InputContextComponent>());
        auto* stateCol = arch->GetColumn(TypeManager::GetComponentTypeID<ResolvedInputStateComponent>());
        if (!userCol || !bindCol || !actionMapCol || !ctxCol || !stateCol) {
            continue;
        }

        for (size_t i = 0; i < arch->GetEntityCount(); ++i) {
            auto& user = *static_cast<InputUserComponent*>(userCol->Get(i));
            auto& bind = *static_cast<InputBindingComponent*>(bindCol->Get(i));
            auto& actionMapComponent = *static_cast<InputActionMapComponent*>(actionMapCol->Get(i));
            auto& ctx = *static_cast<InputContextComponent*>(ctxCol->Get(i));
            auto& state = *static_cast<ResolvedInputStateComponent*>(stateCol->Get(i));

            (void)user;
            (void)bind;
            const InputActionMapAsset* actionMap = ResolveActionMap(actionMapComponent);

            if (ctx.consumed || !ctx.enabled) {
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

            if (actionMap) {
                state.actionCount = static_cast<uint8_t>(
                    actionMap->actions.size() < ResolvedInputStateComponent::MAX_ACTIONS
                        ? actionMap->actions.size()
                        : ResolvedInputStateComponent::MAX_ACTIONS);
                state.axisCount = static_cast<uint8_t>(
                    actionMap->axes.size() < ResolvedInputStateComponent::MAX_AXES
                        ? actionMap->axes.size()
                        : ResolvedInputStateComponent::MAX_AXES);
            } else {
                state.actionCount = 0;
                state.axisCount = 0;
            }

            bool prevHeld[ResolvedInputStateComponent::MAX_ACTIONS] = {};
            for (int a = 0; a < state.actionCount; ++a) {
                prevHeld[a] = state.actions[a].held;
            }

            bool keyDownThisFrame[ResolvedInputStateComponent::MAX_ACTIONS] = {};
            bool keyUpThisFrame[ResolvedInputStateComponent::MAX_ACTIONS] = {};

            state.deltaX = 0.0f;
            state.deltaY = 0.0f;
            state.scrollX = 0.0f;
            state.scrollY = 0.0f;
            state.textLength = 0;
            state.textBuffer[0] = '\0';

            for (const auto& ev : queue.GetEvents()) {
                switch (ev.type) {
                case InputEventType::KeyDown:
                    state.lastDeviceType = InputDeviceType::Keyboard;
                    if (actionMap) {
                        for (int a = 0; a < state.actionCount; ++a) {
                            if (actionMap->actions[a].scancode == ev.key.scancode) {
                                keyDownThisFrame[a] = true;
                            }
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
                                actionMap->actions[a].mouseButton != 0)
                            {
                                keyDownThisFrame[a] = true;
                            }
                        }
                    }
                    break;

                case InputEventType::MouseButtonUp:
                    if (actionMap) {
                        for (int a = 0; a < state.actionCount; ++a) {
                            if (actionMap->actions[a].mouseButton == ev.mouseButton.button &&
                                actionMap->actions[a].mouseButton != 0)
                            {
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
                    break;

                case InputEventType::TextInput:
                {
                    const size_t len = strlen(ev.textInput.text);
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

            for (int a = 0; a < state.actionCount; ++a) {
                auto& action = state.actions[a];
                const bool wasHeld = prevHeld[a];

                if (actionMap) {
                    action.held = IsActionHeld(actionMap->actions[a]);
                    if (keyDownThisFrame[a]) {
                        action.held = true;
                    }
                    if (keyUpThisFrame[a]) {
                        action.held = false;
                    }
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

            for (int ax = 0; ax < state.axisCount; ++ax) {
                float deadzone = 0.15f;
                float sensitivity = 1.0f;
                if (actionMap) {
                    deadzone = actionMap->axes[ax].deadzone;
                    sensitivity = actionMap->axes[ax].sensitivity;
                }
                const float axisValue = actionMap ? ResolveAxisValue(actionMap->axes[ax]) : 0.0f;
                state.axes[ax] = ApplyDeadzone(axisValue, deadzone) * sensitivity;
                if (state.axes[ax] > 1.0f) {
                    state.axes[ax] = 1.0f;
                }
                if (state.axes[ax] < -1.0f) {
                    state.axes[ax] = -1.0f;
                }
            }

            (void)dt;
        }
    }
}
