#pragma once
#include "Entity/Entity.h"
#include "Entity/EntityManager.h"
#include "Archetype/ArchetypeGraph.h"
#include "Entity/EntityRecord.h"
#include "Type/TypeInfo.h"
#include <vector>
#include <new>
#include <utility>

// Entity と Component の対応を管理する ECS の中心クラス。
// EntityManager で EntityID の寿命を管理し、ArchetypeGraph で Component 構成ごとの保存先を切り替える。
class Registry {
public:
    // Registry を初期状態で生成する。
    Registry() = default;

    // Registry が保持する標準コンテナを破棄する。
    ~Registry() = default;

    // 新しい Entity を作成し、空の Archetype に登録する。
    EntityID CreateEntity();

    // 指定 Entity を破棄し、所属 Archetype と EntityManager から取り除く。
    void DestroyEntity(EntityID entity);

    // 指定 Entity が現在も有効かどうかを返す。
    bool IsAlive(EntityID entity) const {
        return m_entityManager.IsAlive(entity);
    }

    // 指定 Entity に Component を追加する。
    // すでに同じ Component を持っている場合は、追加ではなく値の上書きを行う。
    template<typename T>
    void AddComponent(EntityID entity, const T& component) {
        const uint32_t entityIndex = Entity::GetIndex(entity);
        if (Entity::IsNull(entity) ||
            entityIndex >= m_entityRecords.size() ||
            !m_entityManager.IsAlive(entity)) {
            return;
        }

        EntityRecord& record = m_entityRecords[entityIndex];
        if (!record.archetype) {
            return;
        }

        Archetype* oldArchetype = record.archetype;

        ComponentTypeID typeId = TypeManager::GetComponentTypeID<T>();
        if (typeId >= MAX_COMPONENTS) {
            return;
        }

        // すでに Component を持っている場合は、同じ場所の値だけ更新する。
        if (oldArchetype->GetSignature().test(typeId)) {
            ComponentColumn* col = oldArchetype->GetColumn(typeId);
            void* ptr = col->Get(record.row);
            *static_cast<T*>(ptr) = component;
            return;
        }

        // ComponentColumn が型を知らずに構築・移動・破棄できるように、型 T 用の関数を渡す。
        auto constructFn = [](void* dst, const void* src) { new(dst) T(*static_cast<const T*>(src)); };
        auto moveConstructFn = [](void* dst, void* src) { new(dst) T(std::move(*static_cast<T*>(src))); };
        auto moveAssignFn = [](void* dst, void* src) { *static_cast<T*>(dst) = std::move(*static_cast<T*>(src)); };
        auto destructFn = [](void* obj) { static_cast<T*>(obj)->~T(); };

        // Component 追加後の構成を持つ Archetype を取得し、Entity をそこへ移動する。
        Archetype* newArchetype = m_archetypeGraph.GetOrCreateNextArchetype(
            oldArchetype, typeId, sizeof(T), constructFn, moveConstructFn, moveAssignFn, destructFn);

        MoveEntity(entity, oldArchetype, record.row, newArchetype);

        // 新しい Archetype 側の Component 列に、追加する Component の実データを格納する。
        ComponentColumn* newCol = newArchetype->GetColumn(typeId);
        newCol->Add((void*)&component);
    }

    // 指定 Entity から Component を取得する。
    // Entity が無効、または Component を持っていない場合は nullptr を返す。
    template<typename T>
    T* GetComponent(EntityID entity) {
        const uint32_t entityIndex = Entity::GetIndex(entity);
        if (Entity::IsNull(entity) ||
            entityIndex >= m_entityRecords.size() ||
            !m_entityManager.IsAlive(entity)) {
            return nullptr;
        }

        EntityRecord& record = m_entityRecords[entityIndex];
        if (!record.archetype) {
            return nullptr;
        }

        ComponentTypeID typeId = TypeManager::GetComponentTypeID<T>();
        if (typeId >= MAX_COMPONENTS) {
            return nullptr;
        }

        if (!record.archetype->GetSignature().test(typeId)) {
            return nullptr;
        }

        // 所属 Archetype の Component 列から、Entity の行番号に対応する要素を返す。
        ComponentColumn* col = record.archetype->GetColumn(typeId);
        return static_cast<T*>(col->Get(record.row));
    }

    // 指定 Entity から Component を削除する。
    // 削除後の Component 構成を持つ Archetype へ Entity を移動する。
    template<typename T>
    void RemoveComponent(EntityID entity) {
        const uint32_t entityIndex = Entity::GetIndex(entity);
        if (Entity::IsNull(entity) ||
            entityIndex >= m_entityRecords.size() ||
            !m_entityManager.IsAlive(entity)) {
            return;
        }

        EntityRecord& record = m_entityRecords[entityIndex];
        if (!record.archetype) {
            return;
        }

        Archetype* oldArchetype = record.archetype;

        ComponentTypeID typeId = TypeManager::GetComponentTypeID<T>();
        if (typeId >= MAX_COMPONENTS) {
            return;
        }

        if (!oldArchetype->GetSignature().test(typeId)) {
            return;
        }

        // Component 削除後の構成を持つ Archetype を取得し、Entity をそこへ移動する。
        Archetype* newArchetype = m_archetypeGraph.GetOrCreatePreviousArchetype(oldArchetype, typeId);
        MoveEntity(entity, oldArchetype, record.row, newArchetype);
    }

    // Registry が保持している全 Archetype を返す。
    // System 側が条件に合う Archetype を走査するときに使う。
    std::vector<Archetype*> GetAllArchetypes() const {
        return m_archetypeGraph.GetAllArchetypes();
    }

private:
    // Entity を別 Archetype へ移動し、共通 Component のデータも移し替える。
    void MoveEntity(EntityID entity, Archetype* oldArchetype, size_t oldRow, Archetype* newArchetype);

private:
    // EntityID の発行・破棄・世代管理を行う管理器。
    EntityManager m_entityManager;

    // Component 構成ごとの Archetype と、追加・削除時の遷移先を管理するグラフ。
    ArchetypeGraph m_archetypeGraph;

    // Entity index から現在所属している Archetype と行番号を引くためのテーブル。
    std::vector<EntityRecord> m_entityRecords;
};
