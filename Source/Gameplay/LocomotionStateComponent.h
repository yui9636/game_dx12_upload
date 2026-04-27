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
    float walkMaxSpeed = 1.6f;
    float jogMaxSpeed = 3.2f;
    float runMaxSpeed = 5.8f;

    // Physics
    float acceleration = 12.0f;
    float launchBoost = 1.0f;       // Multiplier below 30% max speed
    float deceleration = 18.0f;
    float turnSpeed = 720.0f;       // Degrees/sec

    // Interpretation of moveInput.
    //   true  : camera-relative stick input (Player default).
    //   false : world-space x/z direction (AI / scripted movement).
    // LocomotionSystem skips the camera basis transform when this is false.
    // See EnemyAI_BehaviorTree_Spec_v1.0_2026-04-27.md sections 3.7 / 5.5.
    bool useCameraRelativeInput = true;
};
