#pragma once
#include "Entity/Entity.h"

class Registry;

// Hierarchy ウィンドウの ECS 表示 UI。
// Entity ツリー描画、検索フォーカス要求、D&D による親子変更や
// アセットドロップ生成の入口を担当する。
class HierarchyECSUI {
public:
    // Hierarchy ウィンドウ全体を描画する。
    // registry が nullptr の場合は "No Active Scene" を表示する。
    // p_open はウィンドウ開閉フラグ、outFocused はウィンドウフォーカス状態の出力先。
    static void Render(Registry* registry, bool* p_open = nullptr, bool* outFocused = nullptr);

    // 次フレームで検索ボックスへキーボードフォーカスを移す要求を出す。
    static void RequestSearchFocus();

private:
    // 指定 entity 1 件ぶんのツリーノードを描画する。
    // 子がいれば再帰的に Hierarchy を展開表示する。
    static void DrawEntityNode(Registry* registry, EntityID entity);

    // 指定 parentEntity を D&D ターゲットとして扱う。
    // entity ドロップ時は Reparent、
    // asset ドロップ時は対応する entity 生成や material 割り当てを行う。
    static void HandleDragDropTarget(Registry* registry, EntityID parentEntity = Entity::NULL_ID);
};