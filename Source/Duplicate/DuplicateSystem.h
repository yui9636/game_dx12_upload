#pragma once
#include "Registry/Registry.h"
#include <unordered_map>
#include <vector>

class DuplicateSystem {
public:
    // 単一、または階層構造を丸ごと複製する
    static EntityID Duplicate(EntityID target, Registry& registry);

private:
    // 複製が必要なエンティティをリストアップする（再帰）
    static void CollectHierarchy(EntityID target, Registry& registry, std::vector<EntityID>& outList);
};