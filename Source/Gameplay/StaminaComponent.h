#pragma once

struct StaminaComponent {
    float current = 1000.0f;
    float max = 1000.0f;
    float costPerUse = 250.0f;
    float recoveryRate = 200.0f;
    float recoveryDelay = 0.8f;
    float recoveryTimer = 0.0f;
};
