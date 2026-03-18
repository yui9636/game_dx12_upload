#pragma once
#include "Registry/Registry.h"
#include "PhysicsManager.h"
#include "Component/TransformComponent.h"
#include "Component/PhysicsComponent.h"

class PhysicsSystem {
public:
    // 物理シミュレーションを回し、結果を Transform に同期する
    void Update(Registry& registry, float deltaTime);
};