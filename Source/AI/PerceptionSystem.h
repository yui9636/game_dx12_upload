#pragma once

class Registry;

// Updates AggroComponent.currentTarget and writes target info into BlackboardComponent
// for every enemy with EnemyTagComponent + PerceptionComponent.
//
// v1: distance + FOV only (no raycast).
// Hostile targets: actors with ActorTypeComponent::type == Player.
class PerceptionSystem
{
public:
    static void Update(Registry& registry, float dt);
};
