#pragma once
#include "Entity/Entity.h"
#include "Entity/EntityManager.h"
#include "Archetype/ArchetypeGraph.h"
#include "Entity/EntityRecord.h"
#include "Type/TypeInfo.h"
#include <vector>
#include <new>    
#include <utility> 

class Registry {
public:
    Registry() = default;
    ~Registry() = default;

    // エンティティの発行
    EntityID CreateEntity();

    // エンティティの破棄
    void DestroyEntity(EntityID entity);

    template<typename T>
    void AddComponent(EntityID entity, const T& component) {
        const uint32_t entityIndex = Entity::GetIndex(entity);
        EntityRecord& record = m_entityRecords[entityIndex];
        Archetype* oldArchetype = record.archetype;

        ComponentTypeID typeId = TypeManager::GetComponentTypeID<T>();

        // 1. 既に持っている場合は、値を上書きするだけ
        if (oldArchetype->GetSignature().test(typeId)) {
            ComponentColumn* col = oldArchetype->GetColumn(typeId);
            void* ptr = col->Get(record.row);
            *static_cast<T*>(ptr) = component;
            return;
        }

        auto constructFn = [](void* dst, const void* src) { new(dst) T(*static_cast<const T*>(src)); };
        auto moveConstructFn = [](void* dst, void* src) { new(dst) T(std::move(*static_cast<T*>(src))); };
        auto moveAssignFn = [](void* dst, void* src) { *static_cast<T*>(dst) = std::move(*static_cast<T*>(src)); };
        auto destructFn = [](void* obj) { static_cast<T*>(obj)->~T(); };

        // 2. 持っていない場合、地図（Graph）から引っ越し先のテーブルを取得
        Archetype* newArchetype = m_archetypeGraph.GetOrCreateNextArchetype(
            oldArchetype, typeId, sizeof(T), constructFn, moveConstructFn, moveAssignFn, destructFn);

        // 3. データの引っ越しと、住所録の更新（cpp側で実装）
        MoveEntity(entity, oldArchetype, record.row, newArchetype);

        // 4. 新しいテーブルの新しい行に、追加されたコンポーネントを書き込む
        ComponentColumn* newCol = newArchetype->GetColumn(typeId);

        // =========================================================
        // ★修正：Getして書き込むのではなく、新しく末尾に Add(Push) する！
        // =========================================================
        newCol->Add((void*)&component);
    }



    // コンポーネントの取得（超高速アクセス）
    template<typename T>
    T* GetComponent(EntityID entity) {
        const uint32_t entityIndex = Entity::GetIndex(entity);
        EntityRecord& record = m_entityRecords[entityIndex];

        ComponentTypeID typeId = TypeManager::GetComponentTypeID<T>();

        // 該当の型を持っていなければ nullptr
        if (!record.archetype->GetSignature().test(typeId)) {
            return nullptr;
        }

        ComponentColumn* col = record.archetype->GetColumn(typeId);
        return static_cast<T*>(col->Get(record.row));
    }

    template<typename T>
    void RemoveComponent(EntityID entity) {
        const uint32_t entityIndex = Entity::GetIndex(entity);
        EntityRecord& record = m_entityRecords[entityIndex];
        Archetype* oldArchetype = record.archetype;

        ComponentTypeID typeId = TypeManager::GetComponentTypeID<T>();

        // 持っていない場合は何もしない
        if (!oldArchetype->GetSignature().test(typeId)) {
            return;
        }

        // グラフから「コンポーネントを1つ減らした」引っ越し先のテーブルを取得
        Archetype* newArchetype = m_archetypeGraph.GetOrCreatePreviousArchetype(oldArchetype, typeId);

        // データの引っ越しと住所録の更新（AddComponentと同じ関数が使えます！）
        MoveEntity(entity, oldArchetype, record.row, newArchetype);
    }

    std::vector<Archetype*> GetAllArchetypes() const {
        return m_archetypeGraph.GetAllArchetypes();
    }
private:
    // エンティティを別のテーブルへ移動させ、全データをコピーする中核処理
    void MoveEntity(EntityID entity, Archetype* oldArchetype, size_t oldRow, Archetype* newArchetype);

private:
    EntityManager m_entityManager;
    ArchetypeGraph m_archetypeGraph;

    // インデックスを添字にして住所を引く配列（O(1)アクセス）
    std::vector<EntityRecord> m_entityRecords;
};