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

    EntityID CreateEntity();

    void DestroyEntity(EntityID entity);

    bool IsAlive(EntityID entity) const {
        return m_entityManager.IsAlive(entity);
    }

    template<typename T>
    void AddComponent(EntityID entity, const T& component) {
        const uint32_t entityIndex = Entity::GetIndex(entity);
        EntityRecord& record = m_entityRecords[entityIndex];
        Archetype* oldArchetype = record.archetype;

        ComponentTypeID typeId = TypeManager::GetComponentTypeID<T>();

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

        Archetype* newArchetype = m_archetypeGraph.GetOrCreateNextArchetype(
            oldArchetype, typeId, sizeof(T), constructFn, moveConstructFn, moveAssignFn, destructFn);

        MoveEntity(entity, oldArchetype, record.row, newArchetype);

        ComponentColumn* newCol = newArchetype->GetColumn(typeId);

        // =========================================================
        // =========================================================
        newCol->Add((void*)&component);
    }



    template<typename T>
    T* GetComponent(EntityID entity) {
        const uint32_t entityIndex = Entity::GetIndex(entity);
        EntityRecord& record = m_entityRecords[entityIndex];

        ComponentTypeID typeId = TypeManager::GetComponentTypeID<T>();

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

        if (!oldArchetype->GetSignature().test(typeId)) {
            return;
        }

        Archetype* newArchetype = m_archetypeGraph.GetOrCreatePreviousArchetype(oldArchetype, typeId);

        MoveEntity(entity, oldArchetype, record.row, newArchetype);
    }

    std::vector<Archetype*> GetAllArchetypes() const {
        return m_archetypeGraph.GetAllArchetypes();
    }
private:
    void MoveEntity(EntityID entity, Archetype* oldArchetype, size_t oldRow, Archetype* newArchetype);

private:
    EntityManager m_entityManager;
    ArchetypeGraph m_archetypeGraph;

    std::vector<EntityRecord> m_entityRecords;
};
