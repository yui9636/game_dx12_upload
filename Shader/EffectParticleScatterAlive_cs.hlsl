// ============================================================================
// ScatterAlive: Per-page scatter of alive particle indices into g_AliveList
// Each thread group handles one page. Threads within group scan page slots.
// Dispatch: (activePageCount, 1, 1)
// Uses shared simulation root sig: u0=AliveList
// Page table and header are read via SRV (t0, t1) but since we use shared
// root sig, we read them through the UAV slots that are bound.
// ============================================================================

#include "EffectParticleRuntimeCommon.hlsli"
#include "EffectParticleSoA.hlsli"

// We can't use separate SRVs with the shared root sig easily.
// Instead, read page info from the simulation cbuffer and header from UAV.
// The shader reads: g_BillboardHeader (to check alive), g_AliveList (to write).
// Bind: u0=AliveList, u1=PageAliveOffset(read), u2=BillboardHeader(read)
RWStructuredBuffer<uint>            g_AliveList         : register(u0);
RWStructuredBuffer<uint>            g_PageAliveOffset   : register(u1);
RWStructuredBuffer<BillboardHeader> g_BillboardHeader   : register(u2);

#define GROUP_SIZE 256

groupshared uint s_writeBase;
groupshared uint s_localCount;

[numthreads(GROUP_SIZE, 1, 1)]
void CSMain(uint3 gid : SV_GroupID, uint gtid : SV_GroupIndex)
{
    uint pageIdx = gid.x;
    uint activePageCount = (uint)(gTiming.w + 0.5f);
    if (pageIdx >= activePageCount) return;

    if (gtid == 0u)
    {
        s_writeBase = g_PageAliveOffset[pageIdx];
        s_localCount = 0u;
    }
    GroupMemoryBarrierWithGroupSync();

    uint baseSlot = pageIdx * PAGE_SIZE;

    // Each thread scans multiple slots within this page
    for (uint i = gtid; i < PAGE_SIZE; i += GROUP_SIZE)
    {
        uint slot = baseSlot + i;
        BillboardHeader hdr = g_BillboardHeader[slot];
        if (HeaderIsAlive(hdr.packed))
        {
            uint localIdx;
            InterlockedAdd(s_localCount, 1u, localIdx);
            g_AliveList[s_writeBase + localIdx] = slot;
        }
    }
}
