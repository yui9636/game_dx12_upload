#pragma once

class Registry;

// Reads the per-frame contact list from CollisionManager and converts
// (Attack vs Body) hits into DamageEventComponent::Event records on the
// "_DamageEventQueue" singleton entity.
//
// HealthSystem consumes those events the same frame and clears the queue.
//
// Run order in GameLayer:
//   ... TimelineHitboxSystem -> CollisionSystem -> DamageSystem -> HealthSystem ...
class DamageSystem {
public:
    static void Update(Registry& registry);
};
