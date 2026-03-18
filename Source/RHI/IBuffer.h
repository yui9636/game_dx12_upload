#pragma once
#include <cstdint>

enum class BufferType {
    Vertex,
    Index,
    Constant
};

class IBuffer {
public:
    virtual ~IBuffer() = default;
    virtual uint32_t GetSize() const = 0;
    virtual BufferType GetType() const = 0;

};