#pragma once
#include "Registry/Registry.h"
#include "Component/TransformComponent.h"
#include "Component/HierarchyComponent.h"

class TransformSystem {
public:
    void Update(Registry& registry);

private:
    void ComputeRecursive(EntityID entity, const DirectX::XMMATRIX& parentMatrix, Registry& registry);
};