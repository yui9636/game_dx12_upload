#pragma once
#include <cstdint>
#include <DirectXMath.h>
#include <memory>
#include "RenderQueue.h"   // DrawBatchKey と InstanceData を参照するため。
// Indirect Draw で共有する定数と構造体。
// C++ と HLSL 側の IndirectDrawCommon.hlsli で同じ値を使う。
// 共通定数。
static constexpr uint32_t DRAW_ARGS_STRIDE               = 20;   // DrawArgs のサイズ。
static constexpr uint32_t DRAW_ARGS_INSTANCE_COUNT_OFFSET = 4;    // instanceCount のバイトオフセット。
static constexpr uint32_t INSTANCE_DATA_STRIDE            = 128;  // InstanceData のサイズ。
static constexpr uint32_t CULL_THREAD_GROUP_SIZE          = 64;

// D3D12_DRAW_INDEXED_ARGUMENTS と互換の 20 バイト draw args。
struct DrawArgs {
    uint32_t indexCountPerInstance;
    uint32_t instanceCount;
    uint32_t startIndexLocation;
    int32_t  baseVertexLocation;
    uint32_t startInstanceLocation;
};
static_assert(sizeof(DrawArgs) == DRAW_ARGS_STRIDE, "DrawArgs must be 20 bytes");
static_assert(sizeof(InstanceData) == INSTANCE_DATA_STRIDE, "InstanceData must be 128 bytes");

// CPU 側の描画ループで使う indirect draw メタデータ。
struct IndirectDrawCommand {
    DrawBatchKey              key;
    std::shared_ptr<ModelResource> modelResource;
    uint32_t meshIndex       = 0;   // モデル内の mesh index。
    uint32_t drawArgsIndex   = 0;   // DrawArgs バッファ内の index。
    uint32_t firstInstance   = 0;   // instance バッファ内の開始位置。
    uint32_t instanceCount   = 0;   // instance 数。
    bool     supportsInstancing = false;  // false の場合は skinned mesh。
};

// Compute Culling へ渡す cull command メタデータ。
struct CullCommandMeta {
    uint32_t firstInstance;
    uint32_t instanceCount;
    uint32_t outputInstanceStart;
    uint32_t drawArgsIndex;
    uint32_t indexCount;
    int32_t  baseVertex;
    float    boundsCenterX;
    float    boundsCenterY;
    float    boundsCenterZ;
    float    boundsRadius;
    uint32_t pad[2];
};
static_assert(sizeof(CullCommandMeta) == 48, "CullCommandMeta must be 48 bytes (16-byte aligned)");
