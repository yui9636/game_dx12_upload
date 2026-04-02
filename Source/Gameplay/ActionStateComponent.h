#pragma once
#include <cstdint>

enum class CharacterState : uint8_t {
    Locomotion = 0,
    Action,
    Dodge,
    Jump,
    Damage,
    Dead
};

struct ActionStateComponent {
    CharacterState state = CharacterState::Locomotion;
    int currentNodeIndex = -1;
    int reservedNodeIndex = -1;
    float stateTimer = 0.0f;
    int comboCount = 0;
    float comboTimer = 0.0f;
    float comboTimeout = 3.0f;
};
