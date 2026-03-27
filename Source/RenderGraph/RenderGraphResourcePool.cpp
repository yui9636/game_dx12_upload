#include "RenderGraphResourcePool.h"
#include "RHI/IResourceFactory.h"
#include "Console/Logger.h"
#include <algorithm>

namespace {
    bool IsSameDesc(const TextureDesc& a, const TextureDesc& b) {
        return a.width == b.width &&
            a.height == b.height &&
            a.format == b.format &&
            a.bindFlags == b.bindFlags;
    }
}

std::unique_ptr<ITexture> RenderGraphResourcePool::AcquireTexture(const std::string& name, const TextureDesc& desc, IResourceFactory* factory, uint64_t currentFrame) {
    for (auto& pooled : m_pool) {
        if (pooled.texture && pooled.lastUsedFrame < currentFrame && IsSameDesc(pooled.desc, desc)) {
            pooled.lastUsedFrame = currentFrame;
            // LOG_INFO("  [Pool] Reused Resource: %s", name.c_str());
            return std::move(pooled.texture);
        }
    }

    if (factory) {
        LOG_INFO("  [New] Created Resource: %s", name.c_str());
        return factory->CreateTexture(name, desc);
    }

    return nullptr;
}

void RenderGraphResourcePool::ReleaseTexture(const TextureDesc& desc, std::unique_ptr<ITexture> texture, uint64_t currentFrame) {
    if (texture) {
        m_pool.push_back({ desc, std::move(texture), currentFrame });
    }
}

void RenderGraphResourcePool::Tick(uint64_t currentFrame) {
    m_pool.erase(
        std::remove_if(m_pool.begin(), m_pool.end(),
            [currentFrame](const PooledTexture& p) {
                return !p.texture || (currentFrame - p.lastUsedFrame) > 10;
            }),
        m_pool.end()
    );
}
