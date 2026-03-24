#pragma once
#include "Registry/Registry.h"
#include <unordered_map>
#include <vector>

class DuplicateSystem {
public:
    static EntityID Duplicate(EntityID target, Registry& registry);

private:
    static void CollectHierarchy(EntityID target, Registry& registry, std::vector<EntityID>& outList);
};
