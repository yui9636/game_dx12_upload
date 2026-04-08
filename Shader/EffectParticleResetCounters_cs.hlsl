// ============================================================================
// ResetCounters: Zero per-frame counters and page alive counts
// Dispatch: (totalPages + 63) / 64
// Uses shared simulation root signature. totalPages passed via gTiming.w.
// ============================================================================

#include "EffectParticleRuntimeCommon.hlsli"
#include "EffectParticleSoA.hlsli"

// Shared root sig: u0=Hot, u1=Warm, ...
// We repurpose: u0=Counter, u1=PageAliveCount (only these two are needed)
RWByteAddressBuffer g_CounterBuffer      : register(u0);
RWStructuredBuffer<uint> g_PageAliveCount : register(u1);

[numthreads(64, 1, 1)]
void CSMain(uint3 dtid : SV_DispatchThreadID)
{
    const uint totalPages = (uint)(gTiming.w + 0.5f);

    // Thread 0: reset global counters
    if (dtid.x == 0u)
    {
        g_CounterBuffer.Store(COUNTER_ALIVE_BILLBOARD, 0u);
        g_CounterBuffer.Store(COUNTER_ALIVE_MESH, 0u);
        g_CounterBuffer.Store(COUNTER_ALIVE_RIBBON, 0u);
        g_CounterBuffer.Store(COUNTER_ALIVE_TOTAL, 0u);
        g_CounterBuffer.Store(COUNTER_OVERFLOW, 0u);
        g_CounterBuffer.Store(COUNTER_DROPPED_EMIT, 0u);
    }

    // Zero per-page alive counts
    if (dtid.x < totalPages)
    {
        g_PageAliveCount[dtid.x] = 0u;
    }
}
