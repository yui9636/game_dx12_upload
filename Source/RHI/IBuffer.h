#pragma once
#include <cstdint>

enum class BufferType {
    Vertex,
    Index,
    Constant,
    Indirect,
    Structured,
    UAVStorage   // DEFAULT heap + ALLOW_UNORDERED_ACCESS (DX12 compute output)
};

class IBuffer {
public:
    virtual ~IBuffer() = default;
    virtual uint32_t GetSize() const = 0;
    virtual BufferType GetType() const = 0;
    virtual uint32_t GetStride() const { return 0; }

};