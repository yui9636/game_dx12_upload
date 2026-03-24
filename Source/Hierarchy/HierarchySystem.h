#pragma once
#include "Registry/Registry.h"
#include <unordered_map>

class HierarchySystem {
public:
    void Update(Registry& registry);

private:
    void ComputeWorldMatrix(EntityID entity, Registry& registry);
};
