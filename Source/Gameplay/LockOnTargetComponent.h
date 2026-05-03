#pragma once
#include "Entity/Entity.h"

// Player-side lock-on state. LockOnSystem owns currentTarget;
// the third-person camera reads it via CameraTPVControlComponent.target.
struct LockOnTargetComponent {
    EntityID currentTarget = Entity::NULL_ID;
    float maxRange         = 25.0f;
    float fovRadians       = 1.5708f;
    bool  sticky           = true;
};
