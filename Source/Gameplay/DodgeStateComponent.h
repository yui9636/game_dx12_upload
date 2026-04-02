#pragma once
#include <DirectXMath.h>

struct DodgeStateComponent {
    float dodgeMoveSpeed = 30.0f;       // units/sec during dodge (was 6.0 * 5.0)
    float dodgeDuration = 0.4f;         // Total dodge time in seconds
    float dodgeExitNormalized = 0.9f;   // Exit at this % of duration
    float dodgeTimer = 0.0f;            // Current elapsed time
    float dodgeAngleY = 0.0f;           // Facing angle during dodge (radians)
    bool dodgeTriggered = false;        // Input flag set by PlayerInputSystem
};
