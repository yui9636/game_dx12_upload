#include "DuplicateSystem.h"
#include "Component/TransformComponent.h"
#include "Component/NameComponent.h"
#include "Component/PhysicsComponent.h"
#include "Component/ColliderComponent.h"
#include "Physics/PhysicsManager.h"
#include <System\Query.h>

EntityID DuplicateSystem::Duplicate(EntityID target, Registry& registry) {
    std::vector<EntityID> originalEntities;
    CollectHierarchy(target, registry, originalEntities);

    std::unordered_map<EntityID, EntityID> remapTable;
    for (EntityID oldID : originalEntities) {
        remapTable[oldID] = registry.CreateEntity();
    }

    for (EntityID oldID : originalEntities) {
        EntityID newID = remapTable[oldID];

        if (auto* nameComp = registry.GetComponent<NameComponent>(oldID)) {
            registry.AddComponent<NameComponent>(newID, { nameComp->name + " (Clone)" });
        }

        if (auto* trans = registry.GetComponent<TransformComponent>(oldID)) {
            TransformComponent newTrans = *trans;

            if (remapTable.count(trans->parent)) {
                newTrans.parent = remapTable[trans->parent];
            }

            newTrans.isDirty = true;
            registry.AddComponent<TransformComponent>(newID, newTrans);
        }

        if (auto* collider = registry.GetComponent<ColliderComponent>(oldID)) {
            registry.AddComponent<ColliderComponent>(newID, *collider);

            // BodyID newBodyID = PhysicsManager::Instance().CreateBodyForEntity(newID, *collider, ...);
            // registry.AddComponent<PhysicsComponent>(newID, { newBodyID });
        }
    }

    return remapTable[target];
}

void DuplicateSystem::CollectHierarchy(EntityID target, Registry& registry, std::vector<EntityID>& outList) {
    outList.push_back(target);

    Query<TransformComponent> query(registry);
    query.ForEachWithEntity([&](EntityID entity, TransformComponent& trans) {
        if (trans.parent == target) {
            CollectHierarchy(entity, registry, outList);
        }
        });
}
