#pragma once
#include <cstdint>
#include <string>
#include "RHI/ITexture.h"
// FrameGraph 内のリソースを世代付き index で参照する軽量ハンドル。
struct ResourceHandle {
    uint16_t index   = 0xFFFF;
    uint16_t version = 0;

    bool IsValid() const { return index != 0xFFFF; }

    bool operator==(const ResourceHandle& o) const {
        return index == o.index && version == o.version;
    }
    bool operator!=(const ResourceHandle& o) const { return !(*this == o); }
};
// FrameGraph が生成するテクスチャの作成条件。
struct TextureDesc {
    uint32_t width  = 0;
    uint32_t height = 0;
    TextureFormat   format    = TextureFormat::Unknown;
    TextureBindFlags bindFlags = TextureBindFlags::None;

    float   clearColor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
    float   clearDepth    = 1.0f;
    uint8_t clearStencil  = 0;
};
