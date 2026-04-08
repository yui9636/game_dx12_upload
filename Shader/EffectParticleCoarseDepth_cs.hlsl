// ============================================================================
// CoarseDepthBin: 2-stage group-local depth binning for alpha/premul billboard
// 32 depth bins, log2 intervals, far-to-near render order
// Dispatch: ((alphaAliveCount + 255) / 256, 1, 1)
// ============================================================================

#include "EffectParticleRuntimeCommon.hlsli"
#include "EffectParticleSoA.hlsli"

#define DEPTH_BINS 32u
#define GROUP_SIZE 256u

StructuredBuffer<uint>          g_AliveList     : register(t0);
StructuredBuffer<BillboardHot>  g_BillboardHot  : register(t1);

RWStructuredBuffer<uint>  g_DepthBinIndex   : register(u0); // output: depth-sorted particle indices
RWByteAddressBuffer       g_DepthBinCounter : register(u1); // per-depth-bin global counter (4B * 32)
RWByteAddressBuffer       g_CounterBuffer   : register(u2); // for alive count

cbuffer CoarseDepthParams : register(b0)
{
    float4x4 gViewMatrix;       // world-to-view transform
    float    gNearClip;
    float    gFarClip;
    uint     gAliveCount;
    uint     gPad;
};

groupshared uint s_localDepthCount[DEPTH_BINS];
groupshared uint s_localDepthOffset[DEPTH_BINS]; // global offset for this group's chunk

[numthreads(GROUP_SIZE, 1, 1)]
void CSMain(uint3 dtid : SV_DispatchThreadID, uint3 gid : SV_GroupID, uint gtid : SV_GroupIndex)
{
    // ── Stage 0: Zero local bins ──
    if (gtid < DEPTH_BINS)
    {
        s_localDepthCount[gtid] = 0u;
    }
    GroupMemoryBarrierWithGroupSync();

    // ── Stage 1: Classify and accumulate locally ──
    uint depthBin = 0u;
    bool valid = dtid.x < gAliveCount;
    if (valid)
    {
        uint particleIdx = g_AliveList[dtid.x];
        float3 worldPos = g_BillboardHot[particleIdx].position;
        // Transform to view space to get view-Z depth
        float viewZ = dot(float4(worldPos, 1.0f), gViewMatrix[2]);
        viewZ = abs(viewZ); // ensure positive
        depthBin = ComputeDepthBin(viewZ, gNearClip, gFarClip);

        uint localIdx;
        InterlockedAdd(s_localDepthCount[depthBin], 1u, localIdx);
    }
    GroupMemoryBarrierWithGroupSync();

    // ── Stage 2: One thread per bin does global atomic ──
    if (gtid < DEPTH_BINS)
    {
        uint count = s_localDepthCount[gtid];
        if (count > 0u)
        {
            uint globalOffset;
            g_DepthBinCounter.InterlockedAdd(gtid * 4u, count, globalOffset);
            s_localDepthOffset[gtid] = globalOffset;
        }
        else
        {
            s_localDepthOffset[gtid] = 0u;
        }
    }
    GroupMemoryBarrierWithGroupSync();

    // ── Stage 3: Scatter to depth-sorted position ──
    if (gtid < DEPTH_BINS)
    {
        s_localDepthCount[gtid] = 0u;
    }
    GroupMemoryBarrierWithGroupSync();

    if (valid)
    {
        uint particleIdx = g_AliveList[dtid.x];
        float3 worldPos = g_BillboardHot[particleIdx].position;
        float viewZ = abs(dot(float4(worldPos, 1.0f), gViewMatrix[2]));
        depthBin = ComputeDepthBin(viewZ, gNearClip, gFarClip);

        uint localSlot;
        InterlockedAdd(s_localDepthCount[depthBin], 1u, localSlot);

        uint globalPos = s_localDepthOffset[depthBin] + localSlot;
        g_DepthBinIndex[globalPos] = particleIdx;
    }
}
