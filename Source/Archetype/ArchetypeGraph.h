#pragma once
#include "Archetype.h"
#include "Component/ComponentSignature.h"
#include <unordered_map>
#include <memory>

namespace std {
    template <>
    struct hash<Signature> {
        size_t operator()(const Signature& sig) const {
            return hash<unsigned long long>()(sig.to_ullong());
        }
    };
}

struct ArchetypeEdges {
    std::unordered_map<ComponentTypeID, Archetype*> addEdges;

    std::unordered_map<ComponentTypeID, Archetype*> removeEdges;
};

class ArchetypeGraph {
public:
    ArchetypeGraph() = default;
    ~ArchetypeGraph() = default;

    Archetype* GetEmptyArchetype();

    Archetype* GetOrCreateNextArchetype(Archetype* current, ComponentTypeID typeId, size_t elementSize,
        ComponentColumn::ConstructFn c, ComponentColumn::MoveConstructFn mc,
        ComponentColumn::MoveAssignFn ma, ComponentColumn::DestructFn d);

    Archetype* GetOrCreatePreviousArchetype(Archetype* current, ComponentTypeID typeId);

    std::vector<Archetype*> GetAllArchetypes() const;
private:
    Archetype* GetArchetype(const Signature& sig);

    Archetype* CreateArchetype(const Signature& sig);

private:
    std::unordered_map<Signature, std::unique_ptr<Archetype>> m_archetypes;

    std::unordered_map<Archetype*, ArchetypeEdges> m_edges;

    Archetype* m_emptyArchetype = nullptr;
};
