#pragma once

class Registry;

// AI decision layer.
// For every Enemy entity (EnemyTag + BehaviorTreeAsset + Runtime + Blackboard + SMParams),
// load the .bt file (cached) and tick the tree once per frame.
//
// Writes go to:
//   - LocomotionStateComponent.moveInput / inputStrength / targetAngleY (world-space)
//   - StateMachineParamsComponent (Attack / Dodge are rising-edge)
//   - BlackboardComponent.entries
//
// Never reads/writes other entities' gameplay components except indirectly through Aggro.
class BehaviorTreeSystem
{
public:
    static void Update(Registry& registry, float dt);
    static void InvalidateAssetCache(const char* path = nullptr);
};
