#pragma once
#include <cstdint>
#include "Entity/Entity.h"
// HitboxTrackingComponent は lastHitboxStart/hitEntityCount/hitEntities を保持し、関連システムが実行時状態として参照する。

struct HitboxTrackingComponent {
    int lastHitboxStart = -1;
    uint8_t hitEntityCount = 0;
    EntityID hitEntities[16] = {};

    void ClearHitList() {
        hitEntityCount = 0;
        lastHitboxStart = -1;
    }
};
