#include "DuplicateSystem.h"

#include "Component/HierarchyComponent.h"
#include "Undo/EntitySnapshot.h"

// 指定 entity を subtree ごと複製する。
// ルート名には " (Clone)" を付け、元と同じ親の子として復元する。
EntityID DuplicateSystem::Duplicate(EntityID target, Registry& registry)
{
    // 無効 entity、または既に死んでいる entity は複製できない。
    if (Entity::IsNull(target) || !registry.IsAlive(target)) {
        return Entity::NULL_ID;
    }

    // 対象 subtree 全体を snapshot として取得する。
    EntitySnapshot::Snapshot snapshot = EntitySnapshot::CaptureSubtree(target, registry);
    if (snapshot.nodes.empty()) {
        return Entity::NULL_ID;
    }

    // 元の親 entity を取得しておく。
    EntityID parentEntity = Entity::NULL_ID;
    if (auto* hierarchy = registry.GetComponent<HierarchyComponent>(target)) {
        parentEntity = hierarchy->parent;
    }

    // 複製後のルート名へ " (Clone)" を付ける。
    EntitySnapshot::AppendRootNameSuffix(snapshot, " (Clone)");

    // ルートだけ externalParent を元の親へ向ける。
    for (auto& node : snapshot.nodes) {
        if (node.localID == snapshot.rootLocalID) {
            node.externalParent = parentEntity;
            break;
        }
    }

    // snapshot から subtree を復元し、そのルート entity を返す。
    const EntitySnapshot::RestoreResult restore = EntitySnapshot::RestoreSubtree(snapshot, registry);
    return restore.root;
}

// 指定 entity 以下の階層を収集する。
// 実処理は EntitySnapshot 側の共通関数へ委譲する。
void DuplicateSystem::CollectHierarchy(EntityID target, Registry& registry, std::vector<EntityID>& outList)
{
    EntitySnapshot::CollectHierarchy(target, registry, outList);
}