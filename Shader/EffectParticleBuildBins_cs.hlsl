// ============================================================================
// BuildRendererBins: 2-stage group-local binning
// Stage 1: Thread group accumulates local bin counts
// Stage 2: One global atomic per bin per group (N/256 atomics, not N)
// Stage 3: Scatter particle index to bin-sorted position
// Dispatch: ((aliveCount + 255) / 256, 1, 1)
// ============================================================================

#include "EffectParticleRuntimeCommon.hlsli"
#include "EffectParticleSoA.hlsli"

// MAX_BINS: blendMode(2bit) * sortMode(2bit) = 16 practical bins
// Keep power-of-two for groupshared simplicity
#define MAX_BINS 16u
#define GROUP_SIZE 256u

StructuredBuffer<uint>            g_AliveList       : register(t0);
StructuredBuffer<BillboardWarm>   g_BillboardWarm   : register(t1);
StructuredBuffer<BillboardHeader> g_BillboardHeader : register(t2);

RWStructuredBuffer<uint>  g_BinIndex   : register(u0); // output: bin-sorted particle indices
RWByteAddressBuffer       g_BinCounter : register(u1); // per-bin global counter (4B * MAX_BINS)
RWByteAddressBuffer       g_CounterBuffer : register(u2);

groupshared uint s_localBinCount[MAX_BINS];
groupshared uint s_localBinOffset[MAX_BINS]; // global offset for this group's chunk
groupshared uint s_localPrefix[MAX_BINS];    // exclusive prefix within group

// Extract bin key from Warm flags
// flags layout: blendMode(2) | sortMode(2) | material(8) | subUvFrame(8) | soft(1) | ...
// BinKey = blendMode(2) | sortMode(2) = bits [0:3] of flags
uint ExtractBinKey(uint flags)
{
    return flags & 0xFu; // bottom 4 bits: blendMode(2) + sortMode(2)
}

[numthreads(GROUP_SIZE, 1, 1)]
void CSMain(uint3 dtid : SV_DispatchThreadID, uint3 gid : SV_GroupID, uint gtid : SV_GroupIndex)
{
    // Get alive count
    uint aliveCount = g_CounterBuffer.Load(COUNTER_ALIVE_BILLBOARD);

    // ── Stage 0: Zero local bins ──
    if (gtid < MAX_BINS)
    {
        s_localBinCount[gtid] = 0u;
    }
    GroupMemoryBarrierWithGroupSync();

    // ── Stage 1: Classify and accumulate locally ──
    uint binKey = 0u;
    bool valid = dtid.x < aliveCount;
    if (valid)
    {
        uint particleIdx = g_AliveList[dtid.x];
        BillboardWarm warm = g_BillboardWarm[particleIdx];
        binKey = ExtractBinKey(warm.flags);
        uint localIdx;
        InterlockedAdd(s_localBinCount[binKey], 1u, localIdx);
    }
    GroupMemoryBarrierWithGroupSync();

    // ── Stage 2: One thread per bin does global atomic ──
    if (gtid < MAX_BINS)
    {
        uint count = s_localBinCount[gtid];
        if (count > 0u)
        {
            uint globalOffset;
            g_BinCounter.InterlockedAdd(gtid * 4u, count, globalOffset);
            s_localBinOffset[gtid] = globalOffset;
        }
        else
        {
            s_localBinOffset[gtid] = 0u;
        }

        // Compute local exclusive prefix sum for scatter
        // Simple serial scan - MAX_BINS is tiny (16)
        uint prefix = 0u;
        for (uint b = 0u; b < gtid; ++b)
        {
            prefix += s_localBinCount[b];
        }
        s_localPrefix[gtid] = prefix;
    }
    GroupMemoryBarrierWithGroupSync();

    // ── Stage 3: Scatter to bin-sorted position ──
    // We need each thread to know its position within its bin for this group.
    // Re-do the local atomic to get per-thread local offset within bin.
    // Reset local counts first.
    if (gtid < MAX_BINS)
    {
        s_localBinCount[gtid] = 0u;
    }
    GroupMemoryBarrierWithGroupSync();

    if (valid)
    {
        uint particleIdx = g_AliveList[dtid.x];
        BillboardWarm warm = g_BillboardWarm[particleIdx];
        binKey = ExtractBinKey(warm.flags);

        uint localSlot;
        InterlockedAdd(s_localBinCount[binKey], 1u, localSlot);

        uint globalPos = s_localBinOffset[binKey] + localSlot;
        g_BinIndex[globalPos] = particleIdx;
    }
}
