#pragma once
#include "Entity/Entity.h"

/**
 * @brief 親子関係（階層構造）を保持するコンポーネント。
 * アーキタイプ ECS で vector を使わず高速に参照するためのリンク情報。
 */
struct HierarchyComponent {
    bool isActive = true;
    EntityID parent = Entity::NULL_ID;
    EntityID firstChild = Entity::NULL_ID;
    EntityID prevSibling = Entity::NULL_ID;
    EntityID nextSibling = Entity::NULL_ID;
};

