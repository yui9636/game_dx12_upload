#pragma once
#include <cstdint>
#include <string>
#include "RHI/ITexture.h"
// FrameGraph 内の仮想リソースを参照する軽量ハンドル。
// version は同じ texture を pass が書き換えた世代を表し、依存関係の判定に使う。
struct ResourceHandle {
    uint16_t index   = 0xFFFF;
    uint16_t version = 0;

    bool IsValid() const { return index != 0xFFFF; }

    bool operator==(const ResourceHandle& o) const {
        return index == o.index && version == o.version;
    }
    bool operator!=(const ResourceHandle& o) const { return !(*this == o); }
};
// FrameGraph が一時テクスチャを生成・再利用するときの作成条件。
struct TextureDesc {
    uint32_t width  = 0;
    uint32_t height = 0;
    TextureFormat   format    = TextureFormat::Unknown;
    TextureBindFlags bindFlags = TextureBindFlags::None;

    float   clearColor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
    float   clearDepth    = 1.0f;
    uint8_t clearStencil  = 0;
};
