#include "DuplicateSystem.h"

#include "Component/HierarchyComponent.h"
#include "Undo/EntitySnapshot.h"

EntityID DuplicateSystem::Duplicate(EntityID target, Registry& registry)
{
    if (Entity::IsNull(target) || !registry.IsAlive(target)) {
        return Entity::NULL_ID;
    }

    EntitySnapshot::Snapshot snapshot = EntitySnapshot::CaptureSubtree(target, registry);
    if (snapshot.nodes.empty()) {
        return Entity::NULL_ID;
    }

    EntityID parentEntity = Entity::NULL_ID;
    if (auto* hierarchy = registry.GetComponent<HierarchyComponent>(target)) {
        parentEntity = hierarchy->parent;
    }

    EntitySnapshot::AppendRootNameSuffix(snapshot, " (Clone)");
    for (auto& node : snapshot.nodes) {
        if (node.localID == snapshot.rootLocalID) {
            node.externalParent = parentEntity;
            break;
        }
    }

    const EntitySnapshot::RestoreResult restore = EntitySnapshot::RestoreSubtree(snapshot, registry);
    return restore.root;
}

void DuplicateSystem::CollectHierarchy(EntityID target, Registry& registry, std::vector<EntityID>& outList)
{
    EntitySnapshot::CollectHierarchy(target, registry, outList);
}
