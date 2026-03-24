#pragma once

#include <cstdint>
#include <memory>
#include <vector>

#include "RHI/ITexture.h"

class IResourceFactory;

class PreviewTexturePool {
public:
    void Initialize(IResourceFactory* factory,
                    uint32_t w, uint32_t h,
                    TextureFormat colorFormat,
                    TextureFormat depthFormat,
                    const float* clearColor,
                    uint32_t maxCount);

    std::shared_ptr<ITexture> Acquire();
    void DeferRelease(std::shared_ptr<ITexture> tex, uint64_t fenceValue);
    void ProcessDeferred(uint64_t completedFenceValue);

    uint32_t FreeCount() const { return static_cast<uint32_t>(m_free.size()); }
    uint32_t TotalCreated() const { return m_totalCreated; }
    bool IsFull() const { return m_totalCreated >= m_maxCount && m_free.empty(); }

    ITexture* GetSharedDepth() const { return m_sharedDepth.get(); }

private:
    struct DeferredReturn {
        std::shared_ptr<ITexture> texture;
        uint64_t fenceValue = 0;
    };

    std::vector<std::shared_ptr<ITexture>> m_free;
    std::vector<DeferredReturn> m_deferredReturns;
    std::unique_ptr<ITexture> m_sharedDepth;
    IResourceFactory* m_factory = nullptr;
    uint32_t m_width = 0;
    uint32_t m_height = 0;
    uint32_t m_maxCount = 0;
    uint32_t m_totalCreated = 0;
    TextureFormat m_colorFormat = TextureFormat::Unknown;
    TextureFormat m_depthFormat = TextureFormat::Unknown;
    float m_clearColor[4] = {};
};
