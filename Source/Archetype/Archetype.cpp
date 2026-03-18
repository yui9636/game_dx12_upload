#include "Archetype.h"

Archetype::Archetype(Signature sig) : m_signature(sig) {
}

void Archetype::AddColumn(ComponentTypeID typeId, size_t elementSize,
    ComponentColumn::ConstructFn c, ComponentColumn::MoveConstructFn mc,
    ComponentColumn::MoveAssignFn ma, ComponentColumn::DestructFn d) {

    m_typeToIndex[typeId] = m_columns.size();
    m_columns.emplace_back(elementSize, c, mc, ma, d);

    // スキーマを記憶
    m_schemas[typeId] = { elementSize, c, mc, ma, d };
}

void Archetype::CopySchemaFrom(const Archetype* other) {
    for (const auto& pair : other->m_schemas) {
        AddColumn(pair.first, pair.second.elementSize, pair.second.constructFn, pair.second.moveConstructFn, pair.second.moveAssignFn, pair.second.destructFn);
    }
}

void Archetype::CopySchemaFromExcluding(const Archetype* other, ComponentTypeID excludeTypeId) {
    for (const auto& pair : other->m_schemas) {
        if (pair.first != excludeTypeId) {
            AddColumn(pair.first, pair.second.elementSize, pair.second.constructFn, pair.second.moveConstructFn, pair.second.moveAssignFn, pair.second.destructFn);
        }
    }
}

size_t Archetype::AddEntity(EntityID entity) {
    size_t newIndex = m_entityIDs.size();
    m_entityIDs.push_back(entity);

    // 実際のデータは Registry 側から Add() で各列に流し込まれます。
    // ここでは「何行目に書き込むべきか」の確保だけを行います。

    return newIndex;
}

EntityID Archetype::RemoveEntity(size_t index) {
    if (index >= m_entityIDs.size()) return Entity::NULL_ID;

    // 全ての列に対して Swap And Pop を実行
    for (auto& column : m_columns) {
        column.Remove(index);
    }

    // EntityIDリスト自体も Swap And Pop する
    size_t lastIndex = m_entityIDs.size() - 1;
    EntityID movedEntity = m_entityIDs[lastIndex];

    m_entityIDs[index] = movedEntity;
    m_entityIDs.pop_back();

    return movedEntity;
}

ComponentColumn* Archetype::GetColumn(ComponentTypeID typeId) {
    auto it = m_typeToIndex.find(typeId);
    if (it != m_typeToIndex.end()) {
        return &m_columns[it->second];
    }
    return nullptr;
}