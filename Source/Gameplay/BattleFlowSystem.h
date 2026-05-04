#pragma once

#include <string>

class FlowEventQueue;
class Registry;

// Drives the 1v1 encounter state machine through a runtime service.
// BattleFlowComponent is authoring/config data; the live phase survives
// scene replacement and can be started/reset from GameFlow actions.
//
// Phases:
//   Idle      -> Encounter when the player enters the arena radius
//   Encounter -> Combat after introDuration elapses
//   Combat    -> Victory if the boss dies, Defeat if the player dies
//   Victory/Defeat -> stay until phase is reset externally
//
// The component fields (playerEntity / bossEntity / arenaEntity) are
// designed to be set from the Inspector via the EntityID picker.
class BattleFlowSystem {
public:
    static void Update(Registry& registry, float dt);
    static void Start(const std::string& battleId = std::string{});
    static void Reset();
    static void DrainEvents(FlowEventQueue& outEvents);
};
