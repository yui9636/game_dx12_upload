#pragma once
#include "Registry/Registry.h"
#include "Component/TransformComponent.h"
#include "Component/ColliderComponent.h"
#include "Component/MeshComponent.h"

class CollisionSystem {
public:
    void Update(Registry& registry);
    void Finalize(Registry& registry);
};