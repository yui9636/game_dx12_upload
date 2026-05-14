#pragma once
#include <cstdint>

// DamageSystem が味方判定を除外するために使う陣営タグ。
// 規約: 0 はプレイヤー側、1 は敵側。NPC は 2 以上を使える。
struct TeamComponent {
    uint8_t teamId = 0;
};
