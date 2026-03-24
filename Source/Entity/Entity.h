#pragma once
#include <cstdint>

using EntityID = uint64_t;

namespace Entity {
    constexpr uint32_t INVALID_INDEX = 0xFFFFFFFF;
    constexpr EntityID NULL_ID = 0xFFFFFFFFFFFFFFFF;

    inline EntityID Create(uint32_t index, uint32_t generation) {
        return (static_cast<uint64_t>(index) << 32) | static_cast<uint64_t>(generation);
    }

    inline uint32_t GetIndex(EntityID id) {
        return static_cast<uint32_t>(id >> 32);
    }

    inline uint32_t GetGeneration(EntityID id) {
        return static_cast<uint32_t>(id & 0xFFFFFFFF);
    }

    inline bool IsNull(EntityID id) {
        return id == NULL_ID;
    }
}
