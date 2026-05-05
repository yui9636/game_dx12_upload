#pragma once
#include "Registry/Registry.h"
#include "PhysicsManager.h"
#include "Component/TransformComponent.h"
#include "Component/PhysicsComponent.h"

// Jolt の物理シミュレーションを進め、結果を ECS の Transform に反映するシステム。
class PhysicsSystem {
public:
    // 物理ワールドを更新し、PhysicsComponent を持つ Entity の Transform を同期する。
    void Update(Registry& registry, float deltaTime);
};
