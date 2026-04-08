// ============================================================================
// PrefixSum on g_PageAliveCount -> g_PageAliveOffset
// Work-efficient Blelloch scan (exclusive prefix sum)
// Dispatch: (1, 1, 1) - single group handles up to 2048 pages
// Uses shared simulation root sig: b0=CBV, u0=PageAliveCount, u1=PageAliveOffset, u2=CounterBuffer
// activePageCount passed via gTiming.w (through makeSimulationConstants)
// ============================================================================

#include "EffectParticleRuntimeCommon.hlsli"
#include "EffectParticleSoA.hlsli"

RWStructuredBuffer<uint>       g_PageAliveCount   : register(u0);
RWStructuredBuffer<uint>       g_PageAliveOffset  : register(u1);
RWByteAddressBuffer            g_CounterBuffer    : register(u2);

#define GROUP_SIZE 1024
#define MAX_PAGES  2048  // supports up to 2048 pages = 16M particles

groupshared uint s_data[MAX_PAGES];

[numthreads(GROUP_SIZE, 1, 1)]
void CSMain(uint3 dtid : SV_DispatchThreadID, uint gtid : SV_GroupIndex)
{
    const uint activePageCount = (uint)(gTiming.w + 0.5f);

    // Load phase: each thread loads 2 elements
    uint ai = gtid;
    uint bi = gtid + GROUP_SIZE;

    s_data[ai] = (ai < activePageCount) ? g_PageAliveCount[ai] : 0u;
    s_data[bi] = (bi < activePageCount) ? g_PageAliveCount[bi] : 0u;

    // Up-sweep (reduce)
    uint offset = 1u;
    [unroll]
    for (uint d = MAX_PAGES >> 1u; d > 0u; d >>= 1u)
    {
        GroupMemoryBarrierWithGroupSync();
        if (gtid < d)
        {
            uint idxA = offset * (2u * gtid + 1u) - 1u;
            uint idxB = offset * (2u * gtid + 2u) - 1u;
            s_data[idxB] += s_data[idxA];
        }
        offset <<= 1u;
    }

    // Store total alive count, then clear last element
    if (gtid == 0u)
    {
        uint totalAlive = s_data[MAX_PAGES - 1u];
        g_CounterBuffer.Store(COUNTER_ALIVE_BILLBOARD, totalAlive);
        s_data[MAX_PAGES - 1u] = 0u;
    }

    // Down-sweep
    [unroll]
    for (uint d2 = 1u; d2 < MAX_PAGES; d2 <<= 1u)
    {
        offset >>= 1u;
        GroupMemoryBarrierWithGroupSync();
        if (gtid < d2)
        {
            uint idxA = offset * (2u * gtid + 1u) - 1u;
            uint idxB = offset * (2u * gtid + 2u) - 1u;
            uint tmp = s_data[idxA];
            s_data[idxA] = s_data[idxB];
            s_data[idxB] += tmp;
        }
    }

    GroupMemoryBarrierWithGroupSync();

    // Write results
    if (ai < activePageCount) g_PageAliveOffset[ai] = s_data[ai];
    if (bi < activePageCount) g_PageAliveOffset[bi] = s_data[bi];
}
