#pragma once
#include <cstddef>
#include <cstdint>

class ComponentColumn {
public:
    using ConstructFn = void(*)(void*, const void*);
    using MoveConstructFn = void(*)(void*, void*);
    using MoveAssignFn = void(*)(void*, void*);
    using DestructFn = void(*)(void*);

    ComponentColumn(size_t elementSize, ConstructFn c, MoveConstructFn mc, MoveAssignFn ma, DestructFn d);
    ~ComponentColumn();

    ComponentColumn(const ComponentColumn&) = delete;
    ComponentColumn& operator=(const ComponentColumn&) = delete;

    ComponentColumn(ComponentColumn&& other) noexcept;
    ComponentColumn& operator=(ComponentColumn&& other) noexcept;

    void Add(const void* pData);
    void MoveAdd(void* pData);
    void Remove(size_t index);
    void* Get(size_t index) const;
    size_t GetSize() const { return m_count; }
    void Reserve(size_t newCapacity);

private:
    void* m_data = nullptr;
    size_t m_elementSize = 0;
    size_t m_count = 0;
    size_t m_capacity = 0;

    ConstructFn m_constructFn = nullptr;
    MoveConstructFn m_moveConstructFn = nullptr;
    MoveAssignFn m_moveAssignFn = nullptr;
    DestructFn m_destructFn = nullptr;
};
