#pragma once
#include <cstdint>
#include <cstddef> // size_t

using ComponentTypeID = uint32_t;

// The generated component set already exceeds 64 entries.
// Keep enough headroom so ECS signature tests do not throw while editing/saving prefabs.
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
