#pragma once

#include <cstdint>
#include <memory>
#include <vector>

#include "RHI/ITexture.h"

class IResourceFactory;

class PreviewTexturePool {
public:
    // preview 用 color texture pool と共有 depth texture を初期化する。
    void Initialize(IResourceFactory* factory,
                    uint32_t w, uint32_t h,
                    TextureFormat colorFormat,
                    TextureFormat depthFormat,
                    const float* clearColor,
                    uint32_t maxCount);

    // 空き texture を取得する。足りなければ maxCount まで新規作成する。
    std::shared_ptr<ITexture> Acquire();
    // GPU が使い終える fenceValue まで返却を遅延する。
    void DeferRelease(std::shared_ptr<ITexture> tex, uint64_t fenceValue);
    // 完了済み fence の texture を free list へ戻す。
    void ProcessDeferred(uint64_t completedFenceValue);

    uint32_t FreeCount() const { return static_cast<uint32_t>(m_free.size()); }
    uint32_t TotalCreated() const { return m_totalCreated; }
    bool IsFull() const { return m_totalCreated >= m_maxCount && m_free.empty(); }

    ITexture* GetSharedDepth() const { return m_sharedDepth.get(); }

private:
    struct DeferredReturn {
        std::shared_ptr<ITexture> texture; // GPU 完了待ちの color texture。
        uint64_t fenceValue = 0;           // 返却可能になる fence value。
    };

    std::vector<std::shared_ptr<ITexture>> m_free; // 再利用可能な color texture。
    std::vector<DeferredReturn> m_deferredReturns; // GPU 完了待ちの返却 queue。
    std::unique_ptr<ITexture> m_sharedDepth; // preview 間で共有する depth texture。
    IResourceFactory* m_factory = nullptr; // texture 作成元。非所有。
    uint32_t m_width = 0;      // pool texture 幅。
    uint32_t m_height = 0;     // pool texture 高さ。
    uint32_t m_maxCount = 0;   // pool の最大 color texture 数。
    uint32_t m_totalCreated = 0; // これまで作成した color texture 数。
    TextureFormat m_colorFormat = TextureFormat::Unknown; // color texture format。
    TextureFormat m_depthFormat = TextureFormat::Unknown; // shared depth format。
    float m_clearColor[4] = {}; // color texture の optimized clear value。
};
