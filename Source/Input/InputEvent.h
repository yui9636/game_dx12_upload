#pragma once
#include <cstdint>
#include <cstring>

enum class InputEventType : uint8_t {
    KeyDown, KeyUp,
    MouseMove, MouseButtonDown, MouseButtonUp, MouseWheel,
    GamepadButtonDown, GamepadButtonUp, GamepadAxis,
    TextInput, TextComposition,
    DeviceAdded, DeviceRemoved,
    WindowFocusGained, WindowFocusLost
};

enum class InputDeviceType : uint8_t {
    Unknown, Keyboard, Mouse, Gamepad
};

struct InputDeviceInfo {
    uint32_t deviceId = 0;
    InputDeviceType type = InputDeviceType::Unknown;
    char name[64] = {};
    bool connected = false;
};

struct InputEvent {
    InputEventType type = InputEventType::KeyDown;
    uint32_t deviceId = 0;
    uint64_t timestamp = 0;
    uint16_t sequence = 0;

    union {
        struct { uint32_t scancode; uint32_t keycode; bool repeat; } key;
        struct { float x, y, dx, dy; } mouseMove;
        struct { uint8_t button; float x, y; } mouseButton;
        struct { float scrollX, scrollY; } mouseWheel;
        struct { uint8_t button; } gamepadButton;
        struct { uint8_t axis; float value; } gamepadAxis;
        struct { char text[32]; } textInput;
        struct { char text[32]; int32_t cursor; int32_t selectionLen; } textComposition;
        struct { uint32_t deviceId; InputDeviceType type; } device;
    };

    InputEvent() { memset(&key, 0, sizeof(textComposition)); }
};
