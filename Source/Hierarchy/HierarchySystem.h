#pragma once
#include "Registry/Registry.h"
#include <unordered_map>

class HierarchySystem {
public:
    void Update(Registry& registry);

private:
    // 特定のエンティティのワールド行列を確定させる内部関数
    void ComputeWorldMatrix(EntityID entity, Registry& registry);
};