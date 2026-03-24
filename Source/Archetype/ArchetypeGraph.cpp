#include "ArchetypeGraph.h"

Archetype* ArchetypeGraph::GetEmptyArchetype() {
    if (!m_emptyArchetype) {
        Signature emptySig;
        m_emptyArchetype = CreateArchetype(emptySig);
    }
    return m_emptyArchetype;
}

Archetype* ArchetypeGraph::GetArchetype(const Signature& sig) {
    auto it = m_archetypes.find(sig);
    if (it != m_archetypes.end()) {
        return it->second.get();
    }
    return nullptr;
}

Archetype* ArchetypeGraph::CreateArchetype(const Signature& sig) {
    auto archetype = std::make_unique<Archetype>(sig);
    Archetype* ptr = archetype.get();
    m_archetypes[sig] = std::move(archetype);
    return ptr;
}

Archetype* ArchetypeGraph::GetOrCreateNextArchetype(Archetype* current, ComponentTypeID typeId, size_t elementSize,
    ComponentColumn::ConstructFn c, ComponentColumn::MoveConstructFn mc,
    ComponentColumn::MoveAssignFn ma, ComponentColumn::DestructFn d) {
    auto& edges = m_edges[current];

    auto it = edges.addEdges.find(typeId);
    if (it != edges.addEdges.end()) {
        return it->second;
    }

    Signature nextSig = current->GetSignature();
    nextSig.set(typeId);

    Archetype* nextArchetype = GetArchetype(nextSig);
   
    if (!nextArchetype) {
        nextArchetype = CreateArchetype(nextSig);
        nextArchetype->CopySchemaFrom(current);
        nextArchetype->AddColumn(typeId, elementSize, c, mc, ma, d);
    }

    edges.addEdges[typeId] = nextArchetype;
    m_edges[nextArchetype].removeEdges[typeId] = current;

    return nextArchetype;
}

Archetype* ArchetypeGraph::GetOrCreatePreviousArchetype(Archetype* current, ComponentTypeID typeId) {
    auto& edges = m_edges[current];

    auto it = edges.removeEdges.find(typeId);
    if (it != edges.removeEdges.end()) {
        return it->second;
    }

    Signature nextSig = current->GetSignature();
    nextSig.reset(typeId);

    Archetype* prevArchetype = GetArchetype(nextSig);
    if (!prevArchetype) {
        prevArchetype = CreateArchetype(nextSig);
        prevArchetype->CopySchemaFromExcluding(current, typeId);
    }

    edges.removeEdges[typeId] = prevArchetype;
    m_edges[prevArchetype].addEdges[typeId] = current;

    return prevArchetype;
}

std::vector<Archetype*> ArchetypeGraph::GetAllArchetypes() const {
    std::vector<Archetype*> result;
    result.reserve(m_archetypes.size());

    for (const auto& pair : m_archetypes) {
        result.push_back(pair.second.get());
    }
    return result;
}
