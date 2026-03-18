#pragma once
#include "Entity/Entity.h"

/**
 * @brief 親子関係（階層構造）を保持するコンポーネント
 * アーキタイプECS向けに、std::vectorを使わない双方向リンクリスト方式を採用
 */
struct HierarchyComponent {
    EntityID parent = Entity::NULL_ID;      // 親
    EntityID firstChild = Entity::NULL_ID;  // 最初の子
    EntityID prevSibling = Entity::NULL_ID; // 兄（前の兄弟）
    EntityID nextSibling = Entity::NULL_ID; // 弟（次の兄弟）
};