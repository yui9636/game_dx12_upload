#pragma once
#include "Entity/Entity.h"
#include "Component/ComponentSignature.h"
#include "Component/ComponentColumn.h"
#include <vector>
#include <unordered_map>

class Archetype {
public:
    Archetype(Signature sig);
    ~Archetype() = default;

    Signature GetSignature() const { return m_signature; }

    size_t AddEntity(EntityID entity);

    EntityID RemoveEntity(size_t index);

    void AddColumn(ComponentTypeID typeId, size_t elementSize,
        ComponentColumn::ConstructFn c, ComponentColumn::MoveConstructFn mc,
        ComponentColumn::MoveAssignFn ma, ComponentColumn::DestructFn d);

    void CopySchemaFrom(const Archetype* other);

    void CopySchemaFromExcluding(const Archetype* other, ComponentTypeID excludeTypeId);

    ComponentColumn* GetColumn(ComponentTypeID typeId);

    size_t GetEntityCount() const { return m_entityIDs.size(); }

    const std::vector<EntityID>& GetEntities() const { return m_entityIDs; }

private:
    Signature m_signature;

    std::vector<EntityID> m_entityIDs;

    std::unordered_map<ComponentTypeID, size_t> m_typeToIndex;

    struct ColumnSchema {
        size_t elementSize;
        ComponentColumn::ConstructFn constructFn;
        ComponentColumn::MoveConstructFn moveConstructFn;
        ComponentColumn::MoveAssignFn moveAssignFn;
        ComponentColumn::DestructFn destructFn;
    };
    std::unordered_map<ComponentTypeID, ColumnSchema> m_schemas;


    std::unordered_map<ComponentTypeID, size_t> m_elementSizes;

    std::vector<ComponentColumn> m_columns;
};
