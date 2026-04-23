#include "Archetype.h"

// 指定 signature を持つ archetype を生成する。
// ここでは保持する signature を保存するだけで、列はまだ持たない。
Archetype::Archetype(Signature sig) : m_signature(sig) {
}

// 新しい component 列を archetype に追加する。
// typeId ごとに列 index を記録し、実際の ComponentColumn も生成する。
void Archetype::AddColumn(ComponentTypeID typeId, size_t elementSize,
    ComponentColumn::ConstructFn c, ComponentColumn::MoveConstructFn mc,
    ComponentColumn::MoveAssignFn ma, ComponentColumn::DestructFn d) {

    // typeId から m_columns 内の index を引けるように記録する。
    m_typeToIndex[typeId] = m_columns.size();

    // 実際の component 列を追加する。
    m_columns.emplace_back(elementSize, c, mc, ma, d);

    // 後から schema を複製できるように、生成情報も保存しておく。
    m_schemas[typeId] = { elementSize, c, mc, ma, d };
}

// 他 archetype の列定義をすべてコピーする。
// entity データそのものではなく、「どの component 列を持つか」だけを複製する。
void Archetype::CopySchemaFrom(const Archetype* other) {
    // 相手が持つ全 schema を順に追加する。
    for (const auto& pair : other->m_schemas) {
        AddColumn(pair.first, pair.second.elementSize, pair.second.constructFn, pair.second.moveConstructFn, pair.second.moveAssignFn, pair.second.destructFn);
    }
}

// 他 archetype の列定義をコピーするが、指定した typeId だけは除外する。
// component 削除遷移時の新 archetype 作成で使う想定。
void Archetype::CopySchemaFromExcluding(const Archetype* other, ComponentTypeID excludeTypeId) {
    // 除外対象以外の schema だけを追加する。
    for (const auto& pair : other->m_schemas) {
        if (pair.first != excludeTypeId) {
            AddColumn(pair.first, pair.second.elementSize, pair.second.constructFn, pair.second.moveConstructFn, pair.second.moveAssignFn, pair.second.destructFn);
        }
    }
}

// entity を archetype に追加する。
// 戻り値は、この entity が格納された行 index。
size_t Archetype::AddEntity(EntityID entity) {
    // 追加前の末尾 index が、新しい entity の格納位置になる。
    size_t newIndex = m_entityIDs.size();

    // entity ID を末尾へ追加する。
    m_entityIDs.push_back(entity);

    // 追加された行 index を返す。
    return newIndex;
}

// 指定 index の entity を archetype から削除する。
// 戻り値は、swap-back により index 位置へ移動してきた entity ID。
// 範囲外なら NULL_ID を返す。
EntityID Archetype::RemoveEntity(size_t index) {
    // 範囲外アクセスなら無効 entity を返す。
    if (index >= m_entityIDs.size()) return Entity::NULL_ID;

    // すべての component 列から同じ行を削除する。
    for (auto& column : m_columns) {
        column.Remove(index);
    }

    // 削除前の最後尾 entity を取得する。
    size_t lastIndex = m_entityIDs.size() - 1;
    EntityID movedEntity = m_entityIDs[lastIndex];

    // entity ID 配列側も swap-back と同じ結果に合わせる。
    m_entityIDs[index] = movedEntity;
    m_entityIDs.pop_back();

    // index 位置へ移動してきた entity を返す。
    return movedEntity;
}

// typeId に対応する component 列を取得する。
// 見つからなければ nullptr を返す。
ComponentColumn* Archetype::GetColumn(ComponentTypeID typeId) {
    // typeId から列 index を検索する。
    auto it = m_typeToIndex.find(typeId);

    // 見つかったら対応する列を返す。
    if (it != m_typeToIndex.end()) {
        return &m_columns[it->second];
    }

    // 無ければ nullptr。
    return nullptr;
}