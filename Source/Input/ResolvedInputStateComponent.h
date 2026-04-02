#pragma once
#include <cstdint>
#include "InputEvent.h"

struct ResolvedActionState {
    bool pressed = false;
    bool held = false;
    bool released = false;
    float value = 0.0f;
    int framesSincePressed = 9999;
    int framesSinceReleased = 9999;
};

struct ResolvedInputStateComponent {
    static constexpr int MAX_ACTIONS = 32;
    static constexpr int MAX_AXES = 8;

    ResolvedActionState actions[MAX_ACTIONS] = {};
    float axes[MAX_AXES] = {};
    uint8_t actionCount = 0;
    uint8_t axisCount = 0;

    float pointerX = 0.0f, pointerY = 0.0f;
    float deltaX = 0.0f, deltaY = 0.0f;
    float scrollX = 0.0f, scrollY = 0.0f;

    char textBuffer[256] = {};
    uint16_t textLength = 0;

    InputDeviceType lastDeviceType = InputDeviceType::Unknown;
};
