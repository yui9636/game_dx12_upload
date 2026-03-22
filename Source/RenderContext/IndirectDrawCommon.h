#pragma once
#include <cstdint>
#include <DirectXMath.h>
#include <memory>
#include "RenderQueue.h"   // DrawBatchKey, InstanceData

// ============================================================
// Indirect Draw shared constants and structures
// C++ and HLSL (IndirectDrawCommon.hlsli) use the same values
// ============================================================

// --- Constants ---
static constexpr uint32_t DRAW_ARGS_STRIDE               = 20;   // sizeof(DrawArgs)
static constexpr uint32_t DRAW_ARGS_INSTANCE_COUNT_OFFSET = 4;    // byte offset of instanceCount
static constexpr uint32_t INSTANCE_DATA_STRIDE            = 128;  // sizeof(InstanceData)
static constexpr uint32_t CULL_THREAD_GROUP_SIZE          = 64;

// --- DrawArgs (D3D12_DRAW_INDEXED_ARGUMENTS compatible, 20 bytes) ---
struct DrawArgs {
    uint32_t indexCountPerInstance;
    uint32_t instanceCount;
    uint32_t startIndexLocation;
    int32_t  baseVertexLocation;
    uint32_t startInstanceLocation;
};
static_assert(sizeof(DrawArgs) == DRAW_ARGS_STRIDE, "DrawArgs must be 20 bytes");
static_assert(sizeof(InstanceData) == INSTANCE_DATA_STRIDE, "InstanceData must be 128 bytes");

// --- IndirectDrawCommand (CPU metadata for draw loop) ---
struct IndirectDrawCommand {
    DrawBatchKey              key;
    std::shared_ptr<ModelResource> modelResource;
    uint32_t meshIndex       = 0;   // mesh index within model
    uint32_t drawArgsIndex   = 0;   // index into DrawArgs buffer
    uint32_t firstInstance   = 0;   // start position in instance buffer
    uint32_t instanceCount   = 0;   // number of instances
    bool     supportsInstancing = false;  // false = skinned mesh
};

// --- CullCommandMeta (Compute Culling input, Phase 2) ---
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
