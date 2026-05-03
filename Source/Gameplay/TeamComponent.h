#pragma once
#include <cstdint>

// Faction tag used by DamageSystem to filter friendly fire.
// Convention: 0 = Player side, 1 = Enemy side. NPC may use other ids (>=2).
struct TeamComponent {
    uint8_t teamId = 0;
};
