#include "Registry.h"
#include <cstring>

EntityID Registry::CreateEntity() {
    EntityID entity = m_entityManager.CreateEntity();
    uint32_t index = Entity::GetIndex(entity);

    if (index >= m_entityRecords.size()) {
        m_entityRecords.resize(index + 1);
    }

    Archetype* emptyArchetype = m_archetypeGraph.GetEmptyArchetype();
    size_t row = emptyArchetype->AddEntity(entity);

    m_entityRecords[index] = { emptyArchetype, row };
    return entity;
}

void Registry::DestroyEntity(EntityID entity) {
    if (!m_entityManager.IsAlive(entity)) return;

    uint32_t index = Entity::GetIndex(entity);
    EntityRecord& record = m_entityRecords[index];

    EntityID movedEntity = record.archetype->RemoveEntity(record.row);

    if (!Entity::IsNull(movedEntity) && movedEntity != entity) {
        uint32_t movedIndex = Entity::GetIndex(movedEntity);
        m_entityRecords[movedIndex].row = record.row;
    }

    record.archetype = nullptr;
    m_entityManager.DestroyEntity(entity);
}

// ---------------------------------------------------------
// ---------------------------------------------------------
void Registry::MoveEntity(EntityID entity, Archetype* oldArchetype, size_t oldRow, Archetype* newArchetype) {
    size_t newRow = newArchetype->AddEntity(entity);

    Signature commonSig = oldArchetype->GetSignature() & newArchetype->GetSignature();

    for (uint32_t typeId = 0; typeId < MAX_COMPONENTS; ++typeId) {
        if (commonSig.test(typeId)) {
            ComponentColumn* oldCol = oldArchetype->GetColumn(typeId);
            ComponentColumn* newCol = newArchetype->GetColumn(typeId);
        
            void* src = oldCol->Get(oldRow);
            newCol->MoveAdd(src);

        }
    }

    EntityID movedEntity = oldArchetype->RemoveEntity(oldRow);

    if (!Entity::IsNull(movedEntity) && movedEntity != entity) {
        uint32_t movedIndex = Entity::GetIndex(movedEntity);
        m_entityRecords[movedIndex].row = oldRow;
    }

    uint32_t entityIndex = Entity::GetIndex(entity);
    m_entityRecords[entityIndex].archetype = newArchetype;
    m_entityRecords[entityIndex].row = newRow;
}
