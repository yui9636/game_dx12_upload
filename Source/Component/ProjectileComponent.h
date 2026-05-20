// ProjectileComponent: a single in-flight bullet. Spawned at runtime by
// ProjectileSystem from a ProjectileEmitterComponent; never serialized.
#pragma once
#include <DirectXMath.h>
#include "Entity/Entity.h"

struct ProjectileComponent {
    DirectX::XMFLOAT3 velocity = { 0.0f, 0.0f, 0.0f }; // world units / second
    float lifetime = 5.0f;        // remaining seconds before auto-despawn
    int   damage   = 8;
    float radius   = 0.35f;       // collision radius
    EntityID owner = 0;           // firing entity (never hit by its own bullet)
    bool  targetsPlayer = true;   // true: damages the player, false: damages enemies
};
