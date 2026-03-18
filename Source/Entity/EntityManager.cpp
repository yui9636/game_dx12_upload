#include "EntityManager.h"

EntityID EntityManager::CreateEntity() {
    uint32_t index;
    uint32_t generation;

    if (m_freeIndices.empty()) {
        // 空きがない場合は新しく拡張する
        index = static_cast<uint32_t>(m_generations.size());
        generation = 0;
        m_generations.push_back(generation);
    }
    else {
        // 空きインデックス（以前削除されたもの）を再利用する
        index = m_freeIndices.back();
        m_freeIndices.pop_back();
        generation = m_generations[index];
    }

    m_activeCount++;
    return Entity::Create(index, generation);
}

void EntityManager::DestroyEntity(EntityID entity) {
    const uint32_t index = Entity::GetIndex(entity);

    // 既に破棄されているか、無効なインデックスなら何もしない
    if (index >= m_generations.size() || !IsAlive(entity)) {
        return;
    }

    // ★重要: 世代を1つ進める
    // これにより、昔のIDを持っていたポインタからの IsAlive チェックが false になる
    m_generations[index]++;

    // 空き番号としてスタックに追加
    m_freeIndices.push_back(index);
    m_activeCount--;
}

bool EntityManager::IsAlive(EntityID entity) const {
    const uint32_t index = Entity::GetIndex(entity);

    if (index >= m_generations.size()) {
        return false;
    }

    // 渡されたIDの世代と、現在管理している最新の世代が一致していれば生きている
    return m_generations[index] == Entity::GetGeneration(entity);
}

size_t EntityManager::GetActiveCount() const {
    return m_activeCount;
}