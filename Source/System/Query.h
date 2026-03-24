#pragma once
#include "Registry/Registry.h"
#include "Component/ComponentSignature.h"
#include <vector>
#include <tuple>
#include <type_traits>

template<typename... Ts>
class Query {
public:
    Query(Registry& registry) : m_registry(registry) {
        m_querySignature = CreateSignature<Ts...>();

        for (Archetype* archetype : registry.GetAllArchetypes()) {
            if (SignatureMatches(archetype->GetSignature(), m_querySignature)) {
                m_matchedArchetypes.push_back(archetype);
            }
        }
    }

    template<typename Func>
    void ForEach(Func&& func) {
        for (Archetype* archetype : m_matchedArchetypes) {
            const size_t entityCount = archetype->GetEntityCount();
            if (entityCount == 0) continue;

            auto columnPointers = std::make_tuple(
                static_cast<Ts*>(
                    archetype->GetColumn(TypeManager::GetComponentTypeID<Ts>())->Get(0)
                    )...
            );

            for (size_t i = 0; i < entityCount; ++i) {
                func((std::get<Ts*>(columnPointers)[i])...);
            }
        }
    }

    template<typename Func>
    void ForEachWithEntity(Func&& func) {
        for (Archetype* archetype : m_matchedArchetypes) {
            const size_t entityCount = archetype->GetEntityCount();
            if (entityCount == 0) continue;

            const auto& entities = archetype->GetEntities();

            auto columnPointers = std::make_tuple(
                static_cast<Ts*>(archetype->GetColumn(TypeManager::GetComponentTypeID<Ts>())->Get(0))...
            );

            for (size_t i = 0; i < entityCount; ++i) {
                func(entities[i], (std::get<Ts*>(columnPointers)[i])...);
            }
        }
    }

private:
    Registry& m_registry;
    Signature m_querySignature;
    std::vector<Archetype*> m_matchedArchetypes;
};
