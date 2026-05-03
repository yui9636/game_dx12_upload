#pragma once

class Registry;

// Drives the 1v1 encounter state machine via the singleton
// BattleFlowComponent (created by GameLayer::Initialize).
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
};
