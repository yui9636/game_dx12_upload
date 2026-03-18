#include "Registry.h"
#include <cstring> // memcpy用

EntityID Registry::CreateEntity() {
    EntityID entity = m_entityManager.CreateEntity();
    uint32_t index = Entity::GetIndex(entity);

    // 住所録の配列を拡張
    if (index >= m_entityRecords.size()) {
        m_entityRecords.resize(index + 1);
    }

    // 生まれたてのエンティティは「空のテーブル（コンポーネントゼロ）」に所属する
    Archetype* emptyArchetype = m_archetypeGraph.GetEmptyArchetype();
    size_t row = emptyArchetype->AddEntity(entity);

    m_entityRecords[index] = { emptyArchetype, row };
    return entity;
}

void Registry::DestroyEntity(EntityID entity) {
    if (!m_entityManager.IsAlive(entity)) return;

    uint32_t index = Entity::GetIndex(entity);
    EntityRecord& record = m_entityRecords[index];

    // 所属しているテーブルから削除（内部でSwap&Popが発生する）
    EntityID movedEntity = record.archetype->RemoveEntity(record.row);

    // ★重要: Swap&Pop によって、最後尾にいた別のエンティティが移動してきている。
    // その移動したエンティティの住所録を、穴が空いていた行番号に更新する。
    if (!Entity::IsNull(movedEntity) && movedEntity != entity) {
        uint32_t movedIndex = Entity::GetIndex(movedEntity);
        m_entityRecords[movedIndex].row = record.row;
    }

    // 住所録をクリアし、IDを返却
    record.archetype = nullptr;
    m_entityManager.DestroyEntity(entity);
}

// ---------------------------------------------------------
// これがアーキタイプECSの真髄「引っ越し」ロジックです
// ---------------------------------------------------------
void Registry::MoveEntity(EntityID entity, Archetype* oldArchetype, size_t oldRow, Archetype* newArchetype) {
    // 1. 新しいテーブルに空の行を作成
    size_t newRow = newArchetype->AddEntity(entity);

    // 2. 古いテーブルと新しいテーブルで「共通しているコンポーネント」を全て memcpy で移送
    Signature commonSig = oldArchetype->GetSignature() & newArchetype->GetSignature();

    for (uint32_t typeId = 0; typeId < MAX_COMPONENTS; ++typeId) {
        if (commonSig.test(typeId)) {
            ComponentColumn* oldCol = oldArchetype->GetColumn(typeId);
            ComponentColumn* newCol = newArchetype->GetColumn(typeId);
        
            void* src = oldCol->Get(oldRow);
            newCol->MoveAdd(src);

        }
    }

    // 3. 古いテーブルからエンティティを削除（Swap & Pop）
    EntityID movedEntity = oldArchetype->RemoveEntity(oldRow);

    // 4. Swap & Pop によって場所がズレた「別のエンティティ」の住所録を修正
    if (!Entity::IsNull(movedEntity) && movedEntity != entity) {
        uint32_t movedIndex = Entity::GetIndex(movedEntity);
        m_entityRecords[movedIndex].row = oldRow;
    }

    // 5. 引っ越しした自分自身の住所録を更新
    uint32_t entityIndex = Entity::GetIndex(entity);
    m_entityRecords[entityIndex].archetype = newArchetype;
    m_entityRecords[entityIndex].row = newRow;
}