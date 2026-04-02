#pragma once

class Registry;

// ============================================================================
// Evaluates StateMachineAsset transitions each frame
// Replaces hard-coded ActionSystem/DodgeSystem state transitions
// ============================================================================

class StateMachineSystem
{
public:
    static void Update(Registry& registry, float dt);
};
