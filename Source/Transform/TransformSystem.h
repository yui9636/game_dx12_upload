#pragma once
#include "Registry/Registry.h"
#include "Component/TransformComponent.h"
#include "Component/HierarchyComponent.h"
// TransformSystem は対象コンポーネントを走査し、対応する実行時更新を担当する。

class TransformSystem {
public:
    void Update(Registry& registry);

private:
    void ComputeRecursive(EntityID entity, const DirectX::XMMATRIX& parentMatrix, Registry& registry);
};