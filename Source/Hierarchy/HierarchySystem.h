#pragma once
#include "Registry/Registry.h"
#include <unordered_map>

// Entity の親子関係と Transform の階層更新を担当するシステム。
// 親変更、切り離し、dirty 伝播、worldMatrix 再計算を行う。
class HierarchySystem {
public:
    // Hierarchy 全体を更新する。
    // 親が dirty な子を dirty にし、その後 dirty な transform の worldMatrix を再計算する。
    void Update(Registry& registry);

    // entity を newParent の子にしたとき、循環参照が発生するか判定する。
    static bool WouldCreateCycle(EntityID entity, EntityID newParent, Registry& registry);

    // entity の親を newParent に付け替える。
    // keepWorldTransform が true の場合は、見た目の world 位置を維持するよう localTransform を再計算する。
    static void Reparent(EntityID entity, EntityID newParent, Registry& registry, bool keepWorldTransform = true);

    // entity を現在の親子関係から切り離す。
    static void Detach(EntityID entity, Registry& registry);

    // parent の子として child を接続する。
    // 実体は Reparent のラッパー。
    static void AttachChild(EntityID parent, EntityID child, Registry& registry, bool keepWorldTransform = true);

    // entity 以下の subtree を再帰的に dirty にする。
    static void MarkDirtyRecursive(EntityID entity, Registry& registry);

private:
    // entity の localTransform から worldMatrix を再計算する。
    // 必要なら親の worldMatrix も先に更新する。
    void ComputeWorldMatrix(EntityID entity, Registry& registry);
};