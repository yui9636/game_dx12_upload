#pragma once
#include "Registry/Registry.h"
#include "Component/TransformComponent.h"
#include "Component/ColliderComponent.h"
#include "Component/MeshComponent.h"
// CollisionSystem は対象コンポーネントを走査し、対応する実行時更新を担当する。

class CollisionSystem {
public:
    void Update(Registry& registry);
    void Finalize(Registry& registry);
};