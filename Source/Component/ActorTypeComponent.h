#pragma once

#include <cstdint>

enum class ActorType : uint8_t {
    None    = 0,
    Player  = 1,
    Enemy   = 2,
    NPC     = 3,
    Neutral = 4,
};

// Generic actor identification for gameplay / VFX targeting.
// Coexists with PlayerTagComponent (which is reserved for input device routing).
struct ActorTypeComponent
{
    ActorType type       = ActorType::None;
    uint16_t  factionId  = 0;
};
