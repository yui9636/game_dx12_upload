// Initialize: すべての SoA stream を 0 にし、dead stack を埋め、counter をリセットする。
// ディスパッチ: (capacity + 63) / 64（arena 作成時に 1 回だけ実行）
#include "EffectParticleRuntimeCommon.hlsli"
#include "EffectParticleSoA.hlsli"

RWStructuredBuffer<BillboardHot>    g_BillboardHot     : register(u0);
RWStructuredBuffer<BillboardWarm>   g_BillboardWarm    : register(u1);
RWStructuredBuffer<BillboardCold>   g_BillboardCold    : register(u2);
RWStructuredBuffer<BillboardHeader> g_BillboardHeader  : register(u3);
RWStructuredBuffer<uint>            g_DeadStack        : register(u4);
RWByteAddressBuffer                 g_CounterBuffer    : register(u5);
RWStructuredBuffer<float4>          g_RibbonHistory    : register(u6);

[numthreads(64, 1, 1)]
void CSMain(uint3 dispatchThreadId : SV_DispatchThreadID)
{
    const uint index = dispatchThreadId.x;
    const uint capacity = (uint)gCameraDirectionCapacity.w;
    if (index >= capacity) return;

    // Hot stream を 0 で初期化する。
    BillboardHot hot;
    hot.position = float3(0.0f, 0.0f, 0.0f);
    hot.ageLifePacked = 0u;
    hot.velocity = float3(0.0f, 0.0f, 0.0f);
    hot.sizeSpin = 0u;
    g_BillboardHot[index] = hot;

    // Warm stream を 0 で初期化する。
    BillboardWarm warm;
    warm.packedColor = 0u;
    warm.packedEndColor = 0u;
    warm.texcoordPacked = 0u;
    warm.flags = 0u;
    g_BillboardWarm[index] = warm;

    // Cold stream を 0 で初期化する。
    BillboardCold cold;
    cold.acceleration = float3(0.0f, 0.0f, 0.0f);
    cold.dragSpinPacked = 0u;
    cold.sizeRange = 0u;
    cold.lifeBias = 0u;
    cold.sizeFadeBias = 0u;
    cold.emitterSeed = 0u;
    g_BillboardCold[index] = cold;

    // Header を 0 で初期化する。
    BillboardHeader hdr;
    hdr.slotIndex = index;
    hdr.packed = 0u; // alive は false。
    g_BillboardHeader[index] = hdr;

    // dead stack へ全 slot を dead として積む。
    g_DeadStack[index] = index;

    // ribbon 履歴を初期化する。
    const uint historyBase = index * EffectParticleRibbonHistoryLength;
    [unroll]
    for (uint h = 0u; h < EffectParticleRibbonHistoryLength; ++h)
    {
        g_RibbonHistory[historyBase + h] = float4(0.0f, 0.0f, 0.0f, 1.0f);
    }

    // thread 0 だけが counter を初期化する。
    if (index == 0u)
    {
        g_CounterBuffer.Store(COUNTER_ALIVE_BILLBOARD, 0u);
        g_CounterBuffer.Store(COUNTER_ALIVE_MESH, 0u);
        g_CounterBuffer.Store(COUNTER_ALIVE_RIBBON, 0u);
        g_CounterBuffer.Store(COUNTER_ALIVE_TOTAL, 0u);
        g_CounterBuffer.Store(COUNTER_ALLOCATED_PAGES, 0u);
        g_CounterBuffer.Store(COUNTER_SPARSE_PAGES, 0u);
        g_CounterBuffer.Store(COUNTER_OVERFLOW, 0u);
        g_CounterBuffer.Store(COUNTER_DROPPED_EMIT, 0u);
        g_CounterBuffer.Store(COUNTER_DEAD_STACK_TOP, capacity);

    }
}
