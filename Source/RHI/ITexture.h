#pragma once
#include <cstdint>

enum class TextureFormat {
    Unknown,
    RGBA8_UNORM,
    R16G16B16A16_FLOAT,
    R32G32B32A32_FLOAT,
    R32G32B32A32_UINT,
    R32G32B32_FLOAT,
    R32G32_FLOAT,
    R16G16_FLOAT,
    R8_UNORM,
    D32_FLOAT,
    D24_UNORM_S8_UINT,
    R32_TYPELESS
};


enum class ResourceState {
    Common,
    RenderTarget,
    DepthWrite,
    DepthRead,
    ShaderResource,
    UnorderedAccess,
    CopySource,
    CopyDest,
    Present
};


enum class TextureBindFlags : uint32_t {
    None = 0,
    ShaderResource = 1 << 0,
    RenderTarget = 1 << 1,
    DepthStencil = 1 << 2,
    UnorderedAccess = 1 << 3,
};
inline TextureBindFlags operator|(TextureBindFlags a, TextureBindFlags b) {
    return static_cast<TextureBindFlags>(static_cast<uint32_t>(a) | static_cast<uint32_t>(b));
}
inline bool operator&(TextureBindFlags a, TextureBindFlags b) {
    return (static_cast<uint32_t>(a) & static_cast<uint32_t>(b)) != 0;
}

// =========================================================
// =========================================================
class ITexture {
public:
    virtual ~ITexture() = default;

    virtual uint32_t GetWidth() const = 0;
    virtual uint32_t GetHeight() const = 0;
    virtual TextureFormat GetFormat() const = 0;

    virtual ResourceState GetCurrentState() const = 0;
    virtual void SetCurrentState(ResourceState state) = 0;
};
