#include "Registry.h"
#include <cstring>

// 新しい Entity を作成し、まずは Component を持たない空 Archetype に所属させる。
EntityID Registry::CreateEntity() {
    EntityID entity = m_entityManager.CreateEntity();
    uint32_t index = Entity::GetIndex(entity);

    // Entity index をそのまま EntityRecord の添字に使うため、必要な長さまで拡張する。
    if (index >= m_entityRecords.size()) {
        m_entityRecords.resize(index + 1);
    }

    // Component 追加前の Entity は、空の Archetype に行として登録する。
    Archetype* emptyArchetype = m_archetypeGraph.GetEmptyArchetype();
    size_t row = emptyArchetype->AddEntity(entity);

    // 後から Component 取得や移動ができるように、所属 Archetype と行番号を記録する。
    m_entityRecords[index] = { emptyArchetype, row };
    return entity;
}

// Entity を破棄し、所属 Archetype の行と EntityManager の生存情報を更新する。
void Registry::DestroyEntity(EntityID entity) {
    if (!m_entityManager.IsAlive(entity)) return;

    uint32_t index = Entity::GetIndex(entity);
    EntityRecord& record = m_entityRecords[index];

    // Archetype から行を削除する。末尾 Entity が移動してきた場合は、その EntityID が返る。
    EntityID movedEntity = record.archetype->RemoveEntity(record.row);

    // swap-remove で別 Entity がこの行へ移動した場合は、その EntityRecord の row を更新する。
    if (!Entity::IsNull(movedEntity) && movedEntity != entity) {
        uint32_t movedIndex = Entity::GetIndex(movedEntity);
        m_entityRecords[movedIndex].row = record.row;
    }

    // 破棄済み Entity が古い Archetype を参照し続けないように空にする。
    record.archetype = nullptr;
    m_entityManager.DestroyEntity(entity);
}

// Entity を oldArchetype から newArchetype へ移動する。
// Component の追加・削除時に呼ばれ、両方の Archetype に共通する Component データだけを移す。
void Registry::MoveEntity(EntityID entity, Archetype* oldArchetype, size_t oldRow, Archetype* newArchetype) {
    // 新しい Archetype に Entity 行を先に確保する。
    size_t newRow = newArchetype->AddEntity(entity);

    // 追加・削除されない Component だけを移動対象にする。
    Signature commonSig = oldArchetype->GetSignature() & newArchetype->GetSignature();

    for (uint32_t typeId = 0; typeId < MAX_COMPONENTS; ++typeId) {
        if (commonSig.test(typeId)) {
            ComponentColumn* oldCol = oldArchetype->GetColumn(typeId);
            ComponentColumn* newCol = newArchetype->GetColumn(typeId);

            // 古い列のデータを、新しい列へムーブ構築で追加する。
            void* src = oldCol->Get(oldRow);
            newCol->MoveAdd(src);
        }
    }

    // 古い Archetype から Entity 行を削除する。
    EntityID movedEntity = oldArchetype->RemoveEntity(oldRow);

    // swap-remove で別 Entity が oldRow に移動した場合は、その EntityRecord を修正する。
    if (!Entity::IsNull(movedEntity) && movedEntity != entity) {
        uint32_t movedIndex = Entity::GetIndex(movedEntity);
        m_entityRecords[movedIndex].row = oldRow;
    }

    // 移動した Entity 自身の所属先と行番号を、新しい Archetype 側に更新する。
    uint32_t entityIndex = Entity::GetIndex(entity);
    m_entityRecords[entityIndex].archetype = newArchetype;
    m_entityRecords[entityIndex].row = newRow;
}
