// TimelineProjectileSystem: fires bullet volleys placed on Projectile timeline
// tracks. When a timeline playhead enters a projectile item's frame window the
// volley is spawned via ProjectileSystem::SpawnVolley.
#pragma once

class Registry;

class TimelineProjectileSystem {
public:
    static void Update(Registry& registry, float dt);
};
