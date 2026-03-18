#pragma once
#include "IBind.h" // ★ 基底クラスのインクルード
#include <cstdint>
#include <vector>

class IBuffer;
class ITexture;
class ISampler;

enum class BindingType {
    ConstantBuffer,
    Texture,
    Sampler
};

struct BindGroupLayoutEntry {
    uint32_t binding;
    BindingType type;
};

class IBindGroupLayout {
public:
    virtual ~IBindGroupLayout() = default;
};

struct BindGroupEntry {
    uint32_t binding;
    IBuffer* buffer = nullptr;
    ITexture* texture = nullptr;
    ISampler* sampler = nullptr;
};

// ★ IBind を継承
class IBindGroup : public IBind {
public:
    virtual ~IBindGroup() override = default;
};