#pragma once
#include <vector>
#include <cstdint>
#include <DirectXMath.h>

#include "Entity/Entity.h"

// Per-frame damage event queue. Lives on a singleton entity ("_DamageEventQueue").
// DamageSystem pushes events; HealthSystem consumes and clears them in the same frame.
struct DamageEventComponent {
    struct Event {
        EntityID attacker = Entity::NULL_ID;
        EntityID victim   = Entity::NULL_ID;
        int      amount   = 0;
        DirectX::XMFLOAT3 hitPoint{ 0.0f, 0.0f, 0.0f };
        DirectX::XMFLOAT3 knockbackDir{ 0.0f, 0.0f, 0.0f };
        float    knockbackPower = 0.0f;
        float    hitStopSec     = 0.08f;
        uint8_t  reactionKind   = 0; // 0=light, 1=heavy, 2=launch (v1 = light only)
    };
    std::vector<Event> events;
};
