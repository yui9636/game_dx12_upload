#pragma once
#include "Registry/Registry.h"
#include <unordered_map>
#include <vector>
// DuplicateSystem は対象コンポーネントを走査し、対応する実行時更新を担当する。

class DuplicateSystem {
public:
    static EntityID Duplicate(EntityID target, Registry& registry);

private:
    static void CollectHierarchy(EntityID target, Registry& registry, std::vector<EntityID>& outList);
};
