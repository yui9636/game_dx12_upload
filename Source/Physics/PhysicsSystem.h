#pragma once
#include "Registry/Registry.h"
#include "PhysicsManager.h"
#include "Component/TransformComponent.h"
#include "Component/PhysicsComponent.h"

class PhysicsSystem {
public:
    void Update(Registry& registry, float deltaTime);
};
