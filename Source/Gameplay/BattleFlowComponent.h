#pragma once
#include <cstdint>
#include "Entity/Entity.h"

// Singleton ("_BattleFlow"). Drives the 1v1 encounter state machine.
struct BattleFlowComponent {
    enum class Phase : uint8_t {
        Idle      = 0,
        Encounter = 1,
        Combat    = 2,
        Victory   = 3,
        Defeat    = 4
    };

    Phase    phase           = Phase::Idle;
    float    phaseTimer      = 0.0f;
    EntityID playerEntity    = Entity::NULL_ID;
    EntityID bossEntity      = Entity::NULL_ID;
    EntityID arenaEntity     = Entity::NULL_ID;
    float    encounterRadius = 18.0f;
    float    introDuration   = 1.5f;
};
