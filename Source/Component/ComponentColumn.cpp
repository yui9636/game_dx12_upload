#include "ComponentColumn.h"
#include <cstring>
#include <cstdlib>

ComponentColumn::ComponentColumn(size_t elementSize, ConstructFn c, MoveConstructFn mc, MoveAssignFn ma, DestructFn d)
    : m_elementSize(elementSize), m_count(0), m_capacity(0), m_data(nullptr),
    m_constructFn(c), m_moveConstructFn(mc), m_moveAssignFn(ma), m_destructFn(d) {
}

ComponentColumn::~ComponentColumn() {
    if (m_data) {
        for (size_t i = 0; i < m_count; ++i) {
            m_destructFn(static_cast<char*>(m_data) + i * m_elementSize);
        }
        std::free(m_data);
    }
}

ComponentColumn::ComponentColumn(ComponentColumn&& other) noexcept
    : m_data(other.m_data),
    m_elementSize(other.m_elementSize),
    m_count(other.m_count),
    m_capacity(other.m_capacity),
    m_constructFn(other.m_constructFn),
    m_moveConstructFn(other.m_moveConstructFn),
    m_moveAssignFn(other.m_moveAssignFn),
    m_destructFn(other.m_destructFn)
{
    other.m_data = nullptr;
    other.m_count = 0;
    other.m_capacity = 0;
}

ComponentColumn& ComponentColumn::operator=(ComponentColumn&& other) noexcept {
    if (this != &other) {
        if (m_data) {
            for (size_t i = 0; i < m_count; ++i) {
                m_destructFn(static_cast<char*>(m_data) + i * m_elementSize);
            }
            std::free(m_data);
        }

        m_data = other.m_data;
        m_elementSize = other.m_elementSize;
        m_count = other.m_count;
        m_capacity = other.m_capacity;
        m_constructFn = other.m_constructFn;
        m_moveConstructFn = other.m_moveConstructFn;
        m_moveAssignFn = other.m_moveAssignFn;
        m_destructFn = other.m_destructFn;

        other.m_data = nullptr;
        other.m_count = 0;
        other.m_capacity = 0;
    }
    return *this;
}


void ComponentColumn::Reserve(size_t newCapacity) {
    if (newCapacity <= m_capacity) return;
    void* newData = std::malloc(newCapacity * m_elementSize);
    if (m_data) {
        for (size_t i = 0; i < m_count; ++i) {
            void* src = static_cast<char*>(m_data) + i * m_elementSize;
            void* dst = static_cast<char*>(newData) + i * m_elementSize;
            m_moveConstructFn(dst, src);
            m_destructFn(src);
        }
        std::free(m_data);
    }
    m_data = newData;
    m_capacity = newCapacity;
}

void ComponentColumn::Add(const void* pData) {
    if (m_count >= m_capacity) Reserve(m_capacity == 0 ? 8 : m_capacity * 2);
    void* dst = static_cast<char*>(m_data) + m_count * m_elementSize;
    m_constructFn(dst, pData);
    m_count++;
}

void ComponentColumn::MoveAdd(void* pData) {
    if (m_count >= m_capacity) Reserve(m_capacity == 0 ? 8 : m_capacity * 2);
    void* dst = static_cast<char*>(m_data) + m_count * m_elementSize;
    m_moveConstructFn(dst, pData);
    m_count++;
}



void ComponentColumn::Remove(size_t index) {
    if (index >= m_count) return;
    void* dst = static_cast<char*>(m_data) + (index * m_elementSize);
    size_t lastIndex = m_count - 1;
    if (index != lastIndex) {
        void* src = static_cast<char*>(m_data) + (lastIndex * m_elementSize);
        m_moveAssignFn(dst, src);
        m_destructFn(src);
    }
    else {
        m_destructFn(dst);
    }
    m_count--;
}

void* ComponentColumn::Get(size_t index) const {
    if (index >= m_count) return nullptr;
    return static_cast<char*>(m_data) + (index * m_elementSize);
}
