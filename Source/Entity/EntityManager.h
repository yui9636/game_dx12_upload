#pragma once
#include "Entity.h"
#include <vector>

class EntityManager {
public:
    EntityManager() = default;
    ~EntityManager() = default;

    // 新しいエンティティIDを発行する
    EntityID CreateEntity();

    // エンティティを破棄し、IDを再利用プールに戻す
    void DestroyEntity(EntityID entity);

    // 指定されたIDが現在有効か（古い世代のIDではないか）をチェック
    bool IsAlive(EntityID entity) const;

    // 現在生きているエンティティの総数を取得
    size_t GetActiveCount() const;

private:
    std::vector<uint32_t> m_generations; // 各インデックスの現在の世代を保持
    std::vector<uint32_t> m_freeIndices; // 再利用可能な空きインデックスのスタック
    size_t m_activeCount = 0;            // 生きているエンティティ数
};