#pragma once
#include <cstdint>

struct VibrationRequestComponent {
    float leftMotor = 0.0f;
    float rightMotor = 0.0f;
    float duration = 0.0f; // seconds remaining
    uint8_t targetUserId = 0;
};
