// ProjectileSystem: fires volleys from ProjectileEmitterComponent entities,
// moves live bullets, and resolves bullet-vs-target collisions into damage.
#pragma once
#include <DirectXMath.h>
#include <string>
#include "Entity/Entity.h"

class Registry;

// Settings for a single fired volley. Shared by the auto-emitter and the
// timeline-driven projectile track.
struct ProjectileVolleyParams {
    int   pattern          = 0;     // 0=Aimed, 1=Spread, 2=Ring
    int   bulletsPerVolley = 8;
    float spreadAngleDeg   = 60.0f;
    float bulletSpeed      = 9.0f;
    float bulletLifetime   = 5.0f;
    int   bulletDamage     = 8;
    float bulletRadius     = 0.35f;
    float bulletScale      = 0.3f;
    bool  targetsPlayer    = true;
    std::string bulletModelPath;
};

class ProjectileSystem {
public:
    // Per-frame: emitters fire, bullets move, collisions resolve. Run before
    // DamageSystem / HealthSystem so hits are consumed the same frame.
    static void Update(Registry& registry, float dt);

    // Spawn one volley from `muzzle`, aiming horizontally at the target faction.
    // Must NOT be called while a registry query is iterating.
    static void SpawnVolley(Registry& registry, EntityID owner,
                            const DirectX::XMFLOAT3& muzzle,
                            const ProjectileVolleyParams& params);
};
