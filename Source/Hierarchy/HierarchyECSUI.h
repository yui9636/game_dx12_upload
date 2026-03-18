#pragma once
#include "Entity/Entity.h"

class Registry;

class HierarchyECSUI {
public:
    static void Render(Registry* registry);

private:
    // 再帰的にツリーを描画するヘルパー関数
    static void DrawEntityNode(Registry* registry, EntityID entity);

    // アセットブラウザからのD&Dを受け取り、エンティティを生成する
    static void HandleDragDropTarget(Registry* registry, EntityID parentEntity = Entity::NULL_ID);
};