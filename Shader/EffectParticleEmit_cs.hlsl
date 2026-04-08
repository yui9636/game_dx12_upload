// ============================================================================
// Emit: Pop from dead stack, write to Hot/Warm/Cold SoA streams + Header
// Dispatch: (emitCount, 1, 1) from CPU
// ============================================================================

#include "EffectParticleRuntimeCommon.hlsli"
#include "EffectParticleSoA.hlsli"

RWStructuredBuffer<BillboardHot>    g_BillboardHot    : register(u0);
RWStructuredBuffer<BillboardWarm>   g_BillboardWarm   : register(u1);
RWStructuredBuffer<BillboardCold>   g_BillboardCold   : register(u2);
RWStructuredBuffer<BillboardHeader> g_BillboardHeader  : register(u3);
RWStructuredBuffer<uint>            g_DeadStack        : register(u4);
RWByteAddressBuffer                 g_CounterBuffer    : register(u5);
RWStructuredBuffer<float4>          g_RibbonHistory    : register(u6);
RWStructuredBuffer<uint>            g_PageAliveCount   : register(u7);

[numthreads(64, 1, 1)]
void CSMain(uint3 dispatchThreadId : SV_DispatchThreadID)
{
    const uint emitIndex = dispatchThreadId.x;
    const uint emitCount = (uint)(gTiming.w + 0.5f);
    if (emitIndex >= emitCount) return;

    // Pop from dead stack (atomic decrement)
    uint oldDeadCount = 0u;
    g_CounterBuffer.InterlockedAdd(COUNTER_DEAD_STACK_TOP, 0xFFFFFFFFu, oldDeadCount);
    if (oldDeadCount == 0u)
    {
        g_CounterBuffer.InterlockedAdd(COUNTER_DEAD_STACK_TOP, 1u);
        g_CounterBuffer.InterlockedAdd(COUNTER_OVERFLOW, 1u);
        return;
    }

    const uint slot = g_DeadStack[oldDeadCount - 1u];

    // Unpack cbuffer params
    const uint shapeType = (uint)(gShapeTypeSpinAlphaBias.x + 0.5f);
    const uint seed = (uint)(gSizeSeed.z + 0.5f);
    const float spinRate = gShapeTypeSpinAlphaBias.y;
    const float speed = max(gTiming.z, 0.0f);
    const float particleLifetime = max(gTiming.y, 0.001f);
    const float3 acceleration = gAccelerationDrag.xyz;
    const float drag = max(gAccelerationDrag.w, 0.0f);
    const float startSize = gSizeSeed.x;
    const float endSize = gSizeSeed.y;
    const float sizeBias = gShapeParametersSizeBias.w;
    const float alphaBias = gShapeTypeSpinAlphaBias.z;

    float3 direction = float3(0.0f, 1.0f, 0.0f);
    const float3 spawnOffset = ComputeSpawnOffset(emitIndex, seed + slot * 13u, shapeType, gShapeParametersSizeBias.xyz, direction);
    const float randSpeed = lerp(0.65f, 1.35f, Hash01(seed * 3571u + slot * 2137u + emitIndex * 7417u + 131u));
    const float randSpin = lerp(-spinRate, spinRate, Hash01(seed * 1597u + slot * 6481u + emitIndex * 3253u + 97u));

    // ── Write Hot stream (32B) ──
    BillboardHot hot;
    hot.position = gOriginCurrentTime.xyz + spawnOffset;
    hot.ageLifePacked = PackHalf2(0.0f, particleLifetime);
    hot.velocity = direction * (speed * randSpeed);
    hot.sizeSpin = PackHalf2(startSize, 0.0f);
    g_BillboardHot[slot] = hot;

    // ── Write Warm stream (16B) ──
    BillboardWarm warm;
    warm.packedColor = PackRGBA8(gTint);
    warm.packedEndColor = PackRGBA8(gTintEnd);
    warm.texcoordPacked = PackHalf2(0.0f, 0.0f); // sub-UV computed in update
    warm.flags = 0u; // TODO: blendMode, sortMode from emitter params
    g_BillboardWarm[slot] = warm;

    // ── Write Cold stream (32B, immutable after emit) ──
    BillboardCold cold;
    cold.acceleration = acceleration;
    cold.dragSpinPacked = PackHalf2(drag, randSpin);
    cold.sizeRange = PackHalf2(startSize, endSize);
    cold.lifeBias = PackHalf2(particleLifetime, alphaBias);
    cold.sizeFadeBias = PackHalf2(sizeBias, 0.0f); // fadeBias reserved
    cold.emitterSeed = seed;
    g_BillboardCold[slot] = cold;

    // ── Write Header (8B) ──
    BillboardHeader hdr;
    hdr.slotIndex = slot;
    hdr.packed = HeaderPack(true, 0u, slot / PAGE_SIZE, 0u);
    g_BillboardHeader[slot] = hdr;

    // ── Side-write: per-page alive count (for PrefixSum) ──
    InterlockedAdd(g_PageAliveCount[slot / PAGE_SIZE], 1u);

    // ── Ribbon history init ──
    const float4 posW = float4(hot.position, 1.0f);
    const uint historyBase = slot * EffectParticleRibbonHistoryLength;
    [unroll]
    for (uint h = 0u; h < EffectParticleRibbonHistoryLength; ++h)
    {
        g_RibbonHistory[historyBase + h] = posW;
    }

}
