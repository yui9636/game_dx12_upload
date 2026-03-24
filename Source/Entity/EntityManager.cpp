#include "EntityManager.h"

EntityID EntityManager::CreateEntity() {
    uint32_t index;
    uint32_t generation;

    if (m_freeIndices.empty()) {
        index = static_cast<uint32_t>(m_generations.size());
        generation = 0;
        m_generations.push_back(generation);
    }
    else {
        index = m_freeIndices.back();
        m_freeIndices.pop_back();
        generation = m_generations[index];
    }

    m_activeCount++;
    return Entity::Create(index, generation);
}

void EntityManager::DestroyEntity(EntityID entity) {
    const uint32_t index = Entity::GetIndex(entity);

    if (index >= m_generations.size() || !IsAlive(entity)) {
        return;
    }

    m_generations[index]++;

    m_freeIndices.push_back(index);
    m_activeCount--;
}

bool EntityManager::IsAlive(EntityID entity) const {
    const uint32_t index = Entity::GetIndex(entity);

    if (index >= m_generations.size()) {
        return false;
    }

    return m_generations[index] == Entity::GetGeneration(entity);
}

size_t EntityManager::GetActiveCount() const {
    return m_activeCount;
}
