#pragma once

#include <string>

class FlowEventQueue;
class Registry;

// Drives the 1v1 encounter state machine through a runtime service.
// It is started/reset from GameFlow actions and does not require a
// Hierarchy singleton entity.
//
// Phases:
//   Idle      -> Encounter when the player enters the arena radius
//   Encounter -> Combat after introDuration elapses
//   Combat    -> Victory if the boss dies, Defeat if the player dies
//   Victory/Defeat -> stay until phase is reset externally
//
class BattleFlowSystem {
public:
    static void Update(Registry& registry, float dt);
    static void Start(const std::string& battleId = std::string{});
    static void Reset();
    static void DrainEvents(FlowEventQueue& outEvents);
};
