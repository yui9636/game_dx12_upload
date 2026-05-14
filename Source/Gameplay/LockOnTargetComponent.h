#pragma once
#include "Entity/Entity.h"

// プレイヤー側ロックオン状態。currentTarget は LockOnSystem が所有する。
// 三人称カメラは CameraTPVControlComponent.target 経由でこれを読む。
struct LockOnTargetComponent {
    EntityID currentTarget = Entity::NULL_ID;
    float maxRange         = 25.0f;
    float fovRadians       = 1.5708f;
    bool  sticky           = true;
};
