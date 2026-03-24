#include "PreviewTexturePool.h"

#include <cstring>

#include "RHI/IResourceFactory.h"

void PreviewTexturePool::Initialize(IResourceFactory* factory,
                                    uint32_t w, uint32_t h,
                                    TextureFormat colorFormat,
                                    TextureFormat depthFormat,
                                    const float* clearColor,
                                    uint32_t maxCount)
{
    m_factory = factory;
    m_width = w;
    m_height = h;
    m_colorFormat = colorFormat;
    m_depthFormat = depthFormat;
    m_maxCount = maxCount;
    m_totalCreated = 0;
    m_free.clear();
    m_deferredReturns.clear();
    if (clearColor) {
        memcpy(m_clearColor, clearColor, sizeof(m_clearColor));
    } else {
        memset(m_clearColor, 0, sizeof(m_clearColor));
    }

    m_sharedDepth.reset();
    if (!m_factory) {
        return;
    }

    TextureDesc depthDesc{};
    depthDesc.width = m_width;
    depthDesc.height = m_height;
    depthDesc.format = m_depthFormat;
    depthDesc.bindFlags = TextureBindFlags::DepthStencil;
    depthDesc.clearDepth = 1.0f;
    m_sharedDepth = m_factory->CreateTexture("PreviewPoolDepth", depthDesc);
}

std::shared_ptr<ITexture> PreviewTexturePool::Acquire()
{
    if (!m_free.empty()) {
        auto texture = std::move(m_free.back());
        m_free.pop_back();
        return texture;
    }

    if (!m_factory || m_totalCreated >= m_maxCount) {
        return nullptr;
    }

    TextureDesc desc{};
    desc.width = m_width;
    desc.height = m_height;
    desc.format = m_colorFormat;
    desc.bindFlags = TextureBindFlags::RenderTarget | TextureBindFlags::ShaderResource;
    memcpy(desc.clearColor, m_clearColor, sizeof(desc.clearColor));

    ++m_totalCreated;
    auto raw = m_factory->CreateTexture("PreviewPoolColor", desc);
    return std::shared_ptr<ITexture>(std::move(raw));
}

void PreviewTexturePool::DeferRelease(std::shared_ptr<ITexture> tex, uint64_t fenceValue)
{
    if (!tex) {
        return;
    }
    m_deferredReturns.push_back({ std::move(tex), fenceValue });
}

void PreviewTexturePool::ProcessDeferred(uint64_t completedFenceValue)
{
    auto it = m_deferredReturns.begin();
    while (it != m_deferredReturns.end()) {
        if (it->fenceValue <= completedFenceValue) {
            m_free.push_back(std::move(it->texture));
            it = m_deferredReturns.erase(it);
        } else {
            ++it;
        }
    }
}
