#pragma once
#include <cstdint>
#include <string>
#include "Entity/Entity.h"

// Legacy serialized component. BattleFlow is now driven by BattleFlowSystem's
// runtime state and should not be placed as a Hierarchy singleton.
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
    std::string battleId     = "default";
    bool     autoStartOnPlayerEnter = true;
};
