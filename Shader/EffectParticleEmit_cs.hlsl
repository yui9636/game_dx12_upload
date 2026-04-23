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
RWStructuredBuffer<MeshAttribHot>   g_MeshAttribHot    : register(u8);

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

    const float randomSpeedRange = gRandomParams.x;
    const float randomSizeRange = gRandomParams.y;
    const float randomLifeRange = gRandomParams.z;

    float3 direction = float3(0.0f, 1.0f, 0.0f);
    const float3 spawnOffset = ComputeSpawnOffset(emitIndex, seed + slot * 13u, shapeType, gShapeParametersSizeBias.xyz, direction);
    const float baseSpeedRand = lerp(0.65f, 1.35f, Hash01(seed * 3571u + slot * 2137u + emitIndex * 7417u + 131u));
    const float randSpin = lerp(-spinRate, spinRate, Hash01(seed * 1597u + slot * 6481u + emitIndex * 3253u + 97u));

    // Per-particle random factors (derived from slot hash)
    const float randSpeedFactor = lerp(1.0f - randomSpeedRange, 1.0f + randomSpeedRange, Hash01(seed * 4219u + slot * 8837u + 271u));
    const float randSizeFactor = lerp(1.0f - randomSizeRange, 1.0f + randomSizeRange, Hash01(seed * 5387u + slot * 7193u + 353u));
    const float randLifeFactor = lerp(1.0f - randomLifeRange, 1.0f + randomLifeRange, Hash01(seed * 6571u + slot * 3911u + 479u));

    const float finalSpeed = speed * baseSpeedRand * randSpeedFactor;
    const float finalLife = max(particleLifetime * randLifeFactor, 0.01f);
    const float finalStartSize = startSize * randSizeFactor;
    const float finalEndSize = endSize * randSizeFactor;

    // ── Write Hot stream (32B) ──
    BillboardHot hot;
    hot.position = gOriginCurrentTime.xyz + spawnOffset;
    hot.ageLifePacked = PackHalf2(0.0f, finalLife);
    hot.velocity = direction * finalSpeed;
    hot.sizeSpin = PackHalf2(finalStartSize, 0.0f);
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
    cold.sizeRange = PackHalf2(finalStartSize, finalEndSize);
    cold.lifeBias = PackHalf2(finalLife, alphaBias);
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

    // ── Mesh attribute init (only when Mesh renderer bin) ──
    // gMeshFlags.x: 0=billboard/ribbon (skip), 1=mesh
    if (gMeshFlags.x > 0.5f)
    {
        // Per-particle orientation: base axis/speed + random yaw/pitch/roll offset.
        const float yawRand   = lerp(-gMeshAngularRandomOrient.x, gMeshAngularRandomOrient.x, Hash01(seed * 7919u + slot * 1213u + 61u));
        const float pitchRand = lerp(-gMeshAngularRandomOrient.y, gMeshAngularRandomOrient.y, Hash01(seed * 2683u + slot * 9973u + 139u));
        const float rollRand  = lerp(-gMeshAngularRandomOrient.z, gMeshAngularRandomOrient.z, Hash01(seed * 4093u + slot * 2437u + 239u));
        const float4 initialRotation = QuatNormalize(QuatFromYawPitchRoll(yawRand, pitchRand, rollRand));

        // Scale: base xyz * (1 + random) — uniform random factor per-particle, preserves aspect.
        const float scaleRandRange = max(gMeshInitialScale.w, 0.0f);
        const float scaleRandFactor = lerp(1.0f - scaleRandRange, 1.0f + scaleRandRange, Hash01(seed * 8527u + slot * 5479u + 317u));
        const float3 finalScale = gMeshInitialScale.xyz * scaleRandFactor;

        // Angular speed: base rad/s * (1 + random).
        const float speedRandRange = max(gMeshAngularRandomOrient.w, 0.0f);
        const float speedRandFactor = lerp(1.0f - speedRandRange, 1.0f + speedRandRange, Hash01(seed * 3697u + slot * 6113u + 409u));
        const float finalAngularSpeed = gMeshAngularAxisSpeed.w * speedRandFactor;

        // Normalize axis (cbuffer may hold unnormalized input).
        const float3 axisIn = gMeshAngularAxisSpeed.xyz;
        const float axisLen = length(axisIn);
        const float3 finalAxis = axisLen > 1e-5f ? (axisIn / axisLen) : float3(0.0f, 1.0f, 0.0f);

        MeshAttribHot mattr;
        mattr.rotation      = initialRotation;
        mattr.scale         = finalScale;
        mattr.angularSpeed  = finalAngularSpeed;
        mattr.angularAxis   = finalAxis;
        mattr.rotReserved   = 0.0f;
        mattr.reserved      = uint4(0u, 0u, 0u, 0u);
        g_MeshAttribHot[slot] = mattr;
    }
}
