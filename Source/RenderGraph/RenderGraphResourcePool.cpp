#include "RenderGraphResourcePool.h"
#include "RHI/IResourceFactory.h"
#include "Console/Logger.h"
#include <algorithm>

namespace {
    // IsSameDesc は texture pool の再利用可否を、生成条件が一致するかで判定する。
    bool IsSameDesc(const TextureDesc& a, const TextureDesc& b) {
        return a.width == b.width &&
            a.height == b.height &&
            a.format == b.format &&
            a.bindFlags == b.bindFlags;
    }
}

// RenderGraphResourcePool::AcquireTexture は同じ desc の未使用 texture を再利用し、足りなければ factory で作成する。
std::unique_ptr<ITexture> RenderGraphResourcePool::AcquireTexture(const std::string& name, const TextureDesc& desc, IResourceFactory* factory, uint64_t currentFrame) {
    for (auto& pooled : m_pool) {
        if (pooled.texture && pooled.lastUsedFrame < currentFrame && IsSameDesc(pooled.desc, desc)) {
            pooled.lastUsedFrame = currentFrame;
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

// Tick は transient pool のメモリ肥大を防ぐため、10 frame 以上再利用されていない texture を捨てる。
void RenderGraphResourcePool::Tick(uint64_t currentFrame) {
    m_pool.erase(
        std::remove_if(m_pool.begin(), m_pool.end(),
            [currentFrame](const PooledTexture& p) {
                return !p.texture || (currentFrame - p.lastUsedFrame) > 10;
            }),
        m_pool.end()
    );
}
