#pragma once
#include <cstdint>

// 64bitのエンティティID。
// 上位32bit: インデックス (配列の添字として使用)
// 下位32bit: 世代 (IDが再利用された時の古い参照によるバグ防止用)
using EntityID = uint64_t;

namespace Entity {
    constexpr uint32_t INVALID_INDEX = 0xFFFFFFFF;
    constexpr EntityID NULL_ID = 0xFFFFFFFFFFFFFFFF;

    // インデックスと世代から64bitのIDを生成
    inline EntityID Create(uint32_t index, uint32_t generation) {
        return (static_cast<uint64_t>(index) << 32) | static_cast<uint64_t>(generation);
    }

    // 64bit IDから上位32bit（インデックス）を取り出す
    inline uint32_t GetIndex(EntityID id) {
        return static_cast<uint32_t>(id >> 32);
    }

    // 64bit IDから下位32bit（世代）を取り出す
    inline uint32_t GetGeneration(EntityID id) {
        return static_cast<uint32_t>(id & 0xFFFFFFFF);
    }

    // IDが無効なものかチェック
    inline bool IsNull(EntityID id) {
        return id == NULL_ID;
    }
}