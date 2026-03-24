#pragma once
#include <vector>
#include <memory>
#include <string>
#include "FrameGraphTypes.h"

class ITexture;
class IResourceFactory;

class RenderGraphResourcePool {
public:
    RenderGraphResourcePool() = default;
    ~RenderGraphResourcePool() = default;

    std::unique_ptr<ITexture> AcquireTexture(const std::string& name, const TextureDesc& desc, IResourceFactory* factory, uint64_t currentFrame);

    void ReleaseTexture(const TextureDesc& desc, std::unique_ptr<ITexture> texture, uint64_t currentFrame);

    void Tick(uint64_t currentFrame);

private:
    struct PooledTexture {
        TextureDesc desc;
        std::unique_ptr<ITexture> texture;
        uint64_t lastUsedFrame = 0;
    };

    std::vector<PooledTexture> m_pool;
};
