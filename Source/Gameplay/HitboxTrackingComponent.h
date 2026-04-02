#pragma once
#include <cstdint>
#include "Entity/Entity.h"

struct HitboxTrackingComponent {
    int lastHitboxStart = -1;
    uint8_t hitEntityCount = 0;
    EntityID hitEntities[16] = {};

    void ClearHitList() {
        hitEntityCount = 0;
        lastHitboxStart = -1;
    }
};
