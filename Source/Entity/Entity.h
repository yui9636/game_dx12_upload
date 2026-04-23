#pragma once
#include <cstdint>

// Entity の実体は 64bit 整数。
// 上位 32bit に index、下位 32bit に generation を詰める。
using EntityID = uint64_t;

// EntityID を組み立てたり分解したりする補助関数群。
namespace Entity {

    // 無効な index 値。
    constexpr uint32_t INVALID_INDEX = 0xFFFFFFFF;

    // 無効な EntityID。
    constexpr EntityID NULL_ID = 0xFFFFFFFFFFFFFFFF;

    // index と generation から EntityID を作る。
    inline EntityID Create(uint32_t index, uint32_t generation) {
        return (static_cast<uint64_t>(index) << 32) | static_cast<uint64_t>(generation);
    }

    // EntityID から index 部分を取り出す。
    inline uint32_t GetIndex(EntityID id) {
        return static_cast<uint32_t>(id >> 32);
    }

    // EntityID から generation 部分を取り出す。
    inline uint32_t GetGeneration(EntityID id) {
        return static_cast<uint32_t>(id & 0xFFFFFFFF);
    }

    // NULL_ID かどうかを判定する。
    inline bool IsNull(EntityID id) {
        return id == NULL_ID;
    }
}