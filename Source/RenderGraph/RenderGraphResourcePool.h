#pragma once
#include <vector>
#include <memory>
#include <string>
#include "FrameGraphTypes.h"

class ITexture;
class IResourceFactory;

// FrameGraph の一時 texture を desc ごとに再利用する pool。
// pass 終了後すぐ破棄せず数フレーム保持し、resize などで不要になったものを Tick で掃除する。
class RenderGraphResourcePool {
public:
    RenderGraphResourcePool() = default;
    ~RenderGraphResourcePool() = default;

    // 同じ desc の未使用 texture があれば再利用し、なければ factory で作成する。
    std::unique_ptr<ITexture> AcquireTexture(const std::string& name, const TextureDesc& desc, IResourceFactory* factory, uint64_t currentFrame);

    // FrameGraph 実行後に所有 texture を pool へ戻す。
    void ReleaseTexture(const TextureDesc& desc, std::unique_ptr<ITexture> texture, uint64_t currentFrame);

    // 長期間使われていない texture を破棄する。
    void Tick(uint64_t currentFrame);

private:
    struct PooledTexture {
        TextureDesc desc;
        std::unique_ptr<ITexture> texture;
        uint64_t lastUsedFrame = 0;
    };

    std::vector<PooledTexture> m_pool;
};
