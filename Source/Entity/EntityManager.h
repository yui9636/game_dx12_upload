#pragma once
#include "Entity.h"
#include <vector>

// Entity の生成・破棄・生存判定を担当する管理クラス。
// index 再利用のために free list を持ち、generation で古い参照を無効化する。
class EntityManager {
public:
    // デフォルト構築。
    EntityManager() = default;

    // 特別な破棄処理は不要。
    ~EntityManager() = default;

    // 新しい Entity を生成して返す。
    // 空き index があれば再利用し、無ければ新規 index を追加する。
    EntityID CreateEntity();

    // 指定 Entity を破棄する。
    // generation を進めて古い EntityID を無効化し、index を再利用リストへ戻す。
    void DestroyEntity(EntityID entity);

    // 指定 EntityID が現在も有効かを判定する。
    bool IsAlive(EntityID entity) const;

    // 現在生存している Entity 数を返す。
    size_t GetActiveCount() const;

private:
    // 各 index に対応する現在の generation。
    // EntityID の generation と一致していれば有効とみなす。
    std::vector<uint32_t> m_generations;

    // 破棄済みで再利用可能な index 一覧。
    std::vector<uint32_t> m_freeIndices;

    // 現在生存中の Entity 数。
    size_t m_activeCount = 0;
};