#include "DuplicateSystem.h"
#include "Component/TransformComponent.h"
#include "Component/NameComponent.h"
#include "Component/PhysicsComponent.h"
#include "Component/ColliderComponent.h"
#include "Physics/PhysicsManager.h"
#include <System\Query.h>

EntityID DuplicateSystem::Duplicate(EntityID target, Registry& registry) {
    // 1. 複製対象（自分と子供たちすべて）をリストアップ
    std::vector<EntityID> originalEntities;
    CollectHierarchy(target, registry, originalEntities);

    // 2. IDリマッピング・テーブルの作成 (OldID -> NewID)
    std::unordered_map<EntityID, EntityID> remapTable;
    for (EntityID oldID : originalEntities) {
        remapTable[oldID] = registry.CreateEntity();
    }

    // 3. 各エンティティのコンポーネントをコピー＆修正
    for (EntityID oldID : originalEntities) {
        EntityID newID = remapTable[oldID];

        // --- NameComponent のコピー ---
        if (auto* nameComp = registry.GetComponent<NameComponent>(oldID)) {
            registry.AddComponent<NameComponent>(newID, { nameComp->name + " (Clone)" });
        }

        // --- TransformComponent のコピー & 親子参照の解決 ---
        if (auto* trans = registry.GetComponent<TransformComponent>(oldID)) {
            TransformComponent newTrans = *trans; // バイナリコピー

            // 親が「同時にコピーされたもの」の中にいれば、新IDに書き換える
            if (remapTable.count(trans->parent)) {
                newTrans.parent = remapTable[trans->parent];
            }
            // 親がリスト外（コピーされない親）なら、そのまま（同じ親にぶら下がる）

            newTrans.isDirty = true; // 次のHierarchySystemで再計算させる
            registry.AddComponent<TransformComponent>(newID, newTrans);
        }

        // --- PhysicsComponent / ColliderComponent の処理（物理の再生成） ---
        if (auto* collider = registry.GetComponent<ColliderComponent>(oldID)) {
            // 形状データをコピー
            registry.AddComponent<ColliderComponent>(newID, *collider);

            // Joltの物理ボディを「新EntityID」と共に新規作成する
            // ここで PhysicsManager の既存ロジックを呼び出し、新しい BodyID を取得する
            // ※ここでは簡略化していますが、実際にはJoltのBodyInterfaceを使用します
            // BodyID newBodyID = PhysicsManager::Instance().CreateBodyForEntity(newID, *collider, ...);
            // registry.AddComponent<PhysicsComponent>(newID, { newBodyID });
        }
    }

    return remapTable[target]; // 複製されたルートのIDを返す
}

void DuplicateSystem::CollectHierarchy(EntityID target, Registry& registry, std::vector<EntityID>& outList) {
    outList.push_back(target);

    // 全エンティティから、自分を親に持つものを探す（ここは将来的に高速化の余地あり）
    Query<TransformComponent> query(registry);
    query.ForEachWithEntity([&](EntityID entity, TransformComponent& trans) {
        if (trans.parent == target) {
            CollectHierarchy(entity, registry, outList);
        }
        });
}