#pragma once

#include "Entity/Entity.h"

// Aggro / threat state for an AI entity.
struct AggroComponent
{
    EntityID currentTarget   = Entity::NULL_ID;
    float    threat          = 0.0f;     // 0..1
    float    timeSinceSighted = 0.0f;    // sec; ticks while target is not in sight this frame
    float    loseTargetAfter  = 5.0f;    // sec; clear currentTarget after this much un-sighted time
};
