#pragma once
#include "Entity/Entity.h"

// プレイヤー側ロックオン状態。currentTarget は LockOnSystem が所有する。
// 三人称カメラ自体の追従対象は CameraTPVControlComponent を持つ Entity で決まる。
struct LockOnTargetComponent {
    EntityID currentTarget = Entity::NULL_ID;
    float maxRange         = 25.0f;
    float fovRadians       = 1.5708f;
    bool  sticky           = true;
};
