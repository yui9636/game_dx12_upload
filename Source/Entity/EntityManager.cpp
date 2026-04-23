#include "EntityManager.h"

// 新しい Entity を生成する。
// free list に空き index があれば再利用し、無ければ新規 index を発行する。
EntityID EntityManager::CreateEntity() {
    uint32_t index;
    uint32_t generation;

    // 使い回せる index が無ければ末尾に新規追加する。
    if (m_freeIndices.empty()) {
        index = static_cast<uint32_t>(m_generations.size());
        generation = 0;
        m_generations.push_back(generation);
    }
    else {
        // 破棄済み entity の index を再利用する。
        index = m_freeIndices.back();
        m_freeIndices.pop_back();
        generation = m_generations[index];
    }

    // 生存 entity 数を増やして EntityID を返す。
    m_activeCount++;
    return Entity::Create(index, generation);
}

// Entity を破棄する。
// generation を 1 増やして古い handle を無効化し、index を free list へ戻す。
void EntityManager::DestroyEntity(EntityID entity) {
    const uint32_t index = Entity::GetIndex(entity);

    // 範囲外、または既に死んでいる entity なら何もしない。
    if (index >= m_generations.size() || !IsAlive(entity)) {
        return;
    }

    // generation を進めて、古い EntityID を無効化する。
    m_generations[index]++;

    // index を再利用可能リストへ戻す。
    m_freeIndices.push_back(index);

    // 生存 entity 数を減らす。
    m_activeCount--;
}

// 指定 EntityID が現在も有効かを判定する。
// index が有効範囲内で、generation が一致していれば生存中とみなす。
bool EntityManager::IsAlive(EntityID entity) const {
    const uint32_t index = Entity::GetIndex(entity);

    // index が範囲外なら無効。
    if (index >= m_generations.size()) {
        return false;
    }

    // generation 一致で生存判定する。
    return m_generations[index] == Entity::GetGeneration(entity);
}

// 現在生存している entity 数を返す。
size_t EntityManager::GetActiveCount() const {
    return m_activeCount;
}