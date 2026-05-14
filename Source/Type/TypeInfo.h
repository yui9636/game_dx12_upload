#pragma once
#include <cstdint>
#include <cstddef> // size_t 定義用。

using ComponentTypeID = uint32_t;

// 生成済みコンポーネント数がすでに 64 を超える。
// Prefab の編集や保存中に ECS signature の検査で例外が出ないよう余裕を持たせる。
constexpr uint32_t MAX_COMPONENTS = 128;

struct ComponentMetadata {
    size_t size;
    size_t alignment;
};

class TypeManager {
private:
    static ComponentTypeID GetNextTypeID() {
        static ComponentTypeID s_componentCounter = 0;
        return s_componentCounter++;
    }

public:
    template <typename T>
    static ComponentTypeID GetComponentTypeID() {
        static const ComponentTypeID id = GetNextTypeID();
        return id;
    }

    template <typename T>
    static ComponentMetadata GetComponentMetadata() {
        return { sizeof(T), alignof(T) };
    }
};
