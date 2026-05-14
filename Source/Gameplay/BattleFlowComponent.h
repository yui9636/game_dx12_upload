#pragma once
#include <cstdint>
#include <string>
#include "Entity/Entity.h"

// 旧シリアライズ用 component。BattleFlow は現在 BattleFlowSystem の
// 実行時状態で駆動されるため、Hierarchy singleton として配置しない。
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
