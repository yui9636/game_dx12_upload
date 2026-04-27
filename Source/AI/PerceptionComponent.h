#pragma once
#include <cstdint>

// Sight / hearing parameters for AI perception.
struct PerceptionComponent
{
    // Sight
    bool  sightEnabled        = true;
    float sightRadius         = 10.0f;     // metres
    float sightFOV            = 1.5708f;   // 90 deg in radians
    float sightHeight         = 1.6f;      // ray origin offset (m)
    bool  requireLineOfSight  = false;     // v1: distance + FOV only

    // Hearing (v1 stub: radius only, events come later)
    bool  hearingEnabled      = false;
    float hearingRadius       = 6.0f;

    // Detection target faction (0 = treat ActorType::Player as hostile by default)
    uint16_t targetFactionMask = 0;
};
