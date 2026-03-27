#pragma once
#include "Registry/Registry.h"
#include <unordered_map>

class HierarchySystem {
public:
    void Update(Registry& registry);

    static bool WouldCreateCycle(EntityID entity, EntityID newParent, Registry& registry);
    static void Reparent(EntityID entity, EntityID newParent, Registry& registry, bool keepWorldTransform = true);
    static void Detach(EntityID entity, Registry& registry);
    static void AttachChild(EntityID parent, EntityID child, Registry& registry, bool keepWorldTransform = true);
    static void MarkDirtyRecursive(EntityID entity, Registry& registry);

private:
    void ComputeWorldMatrix(EntityID entity, Registry& registry);
};
