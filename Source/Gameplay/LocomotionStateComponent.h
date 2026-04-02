#pragma once
#include <DirectXMath.h>

struct LocomotionStateComponent {
    DirectX::XMFLOAT2 moveInput = { 0, 0 };   // Raw stick input
    float inputStrength = 0.0f;                 // Magnitude [0,1]
    DirectX::XMFLOAT2 worldMoveDir = { 0, 0 }; // Camera-relative world direction (normalized)

    uint8_t gaitIndex = 0;       // 0=Idle, 1=Walk, 2=Jog, 3=Run
    float currentSpeed = 0.0f;   // Actual movement speed

    float targetAngleY = 0.0f;   // Desired facing angle (radians)
    bool turningInPlace = false;
    int lastTurnSign = 0;        // +1 right, -1 left

    // Gait thresholds (input strength)
    float walkThreshold = 0.10f;
    float jogThreshold = 0.35f;
    float runThreshold = 0.90f;

    // Max speeds per gait
    float walkMaxSpeed = 60.0f;
    float jogMaxSpeed = 160.0f;
    float runMaxSpeed = 380.0f;

    // Physics
    float acceleration = 60.0f;
    float launchBoost = 5.0f;       // Multiplier below 30% max speed
    float deceleration = 2000.0f;
    float turnSpeed = 1080.0f;      // Degrees/sec
};
