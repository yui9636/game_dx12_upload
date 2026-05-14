#pragma once

#include <cstdint>

enum class ActorType : uint8_t {
    None    = 0,
    Player  = 1,
    Enemy   = 2,
    NPC     = 3,
    Neutral = 4,
};

// gameplay と VFX のターゲット解決に使う汎用 actor 識別。
// 入力デバイス routing 用の PlayerTagComponent と併用する。
struct ActorTypeComponent
{
    ActorType type       = ActorType::None;
    uint16_t  factionId  = 0;
};
