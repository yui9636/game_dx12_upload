// ProjectileEmitterComponent: a bullet emitter placed on an entity (enemy, or
// the player's pod). ProjectileSystem fires volleys from it on a fixed interval.
#pragma once
#include <DirectXMath.h>
#include <string>

struct ProjectileEmitterComponent {
    bool  active = true;            // when false, the emitter holds fire

    // Firing pattern: 0 = Aimed (1 bullet at target),
    //                 1 = Spread (fan of bullets toward target),
    //                 2 = Ring   (bullets evenly around 360 degrees)
    int   pattern = 0;

    float fireInterval    = 1.2f;   // seconds between volleys
    int   bulletsPerVolley = 8;     // bullets per volley (Spread / Ring)
    float spreadAngleDeg  = 60.0f;  // fan width in degrees (Spread)

    float bulletSpeed     = 9.0f;   // world units / second
    float bulletLifetime  = 5.0f;   // seconds before a bullet despawns
    int   bulletDamage    = 8;
    float bulletRadius    = 0.35f;  // bullet collision radius
    float bulletScale     = 0.3f;   // uniform scale for the bullet model

    bool  targetsPlayer   = true;   // true: enemy emitter (bullets hit the player)

    DirectX::XMFLOAT3 muzzleOffset = { 0.0f, 1.2f, 0.0f }; // local-space muzzle offset
    std::string bulletModelPath;    // model asset drawn for each bullet

    float fireTimer = 0.0f;         // runtime countdown to the next volley
};
