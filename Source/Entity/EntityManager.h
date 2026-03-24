#pragma once
#include "Entity.h"
#include <vector>

class EntityManager {
public:
    EntityManager() = default;
    ~EntityManager() = default;

    EntityID CreateEntity();

    void DestroyEntity(EntityID entity);

    bool IsAlive(EntityID entity) const;

    size_t GetActiveCount() const;

private:
    std::vector<uint32_t> m_generations;
    std::vector<uint32_t> m_freeIndices;
    size_t m_activeCount = 0;
};
