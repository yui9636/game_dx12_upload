#ifndef INDIRECT_DRAW_COMMON_HLSLI
#define INDIRECT_DRAW_COMMON_HLSLI

// ============================================================
// Indirect Draw 共通定数 (HLSL)
// C++ 側: IndirectDrawCommon.h と同じ値
// ============================================================

#define DRAW_ARGS_STRIDE               20
#define DRAW_ARGS_INSTANCE_COUNT_OFFSET 4
#define INSTANCE_DATA_STRIDE           128
#define CULL_THREAD_GROUP_SIZE         64

#endif // INDIRECT_DRAW_COMMON_HLSLI
