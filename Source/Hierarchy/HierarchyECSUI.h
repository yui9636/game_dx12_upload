#pragma once
#include "Entity/Entity.h"

class Registry;

class HierarchyECSUI {
public:
    static void Render(Registry* registry);

private:
    static void DrawEntityNode(Registry* registry, EntityID entity);

    static void HandleDragDropTarget(Registry* registry, EntityID parentEntity = Entity::NULL_ID);
};
