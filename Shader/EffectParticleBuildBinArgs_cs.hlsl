// ============================================================================
// BuildBinArgs: Read per-bin counters, write D3D12_DRAW_ARGUMENTS per bin
// Also compute bin offsets (exclusive prefix sum) for rendering
// Dispatch: (1, 1, 1)
// ============================================================================

#include "EffectParticleSoA.hlsli"

#define MAX_BINS 16u

RWByteAddressBuffer g_BinCounter   : register(u0); // input: per-bin counts (4B * MAX_BINS)
RWByteAddressBuffer g_IndirectArgs : register(u1); // output: D3D12_DRAW_ARGUMENTS per bin (16B * MAX_BINS)
RWByteAddressBuffer g_BinOffset    : register(u2); // output: exclusive prefix sum per bin (4B * MAX_BINS)

[numthreads(1, 1, 1)]
void CSMain(uint3 dtid : SV_DispatchThreadID)
{
    // Serial prefix sum over MAX_BINS (tiny, single thread is fine)
    uint runningOffset = 0u;

    for (uint bin = 0u; bin < MAX_BINS; ++bin)
    {
        uint count = g_BinCounter.Load(bin * 4u);

        // Write exclusive prefix (start offset for this bin in g_BinIndex)
        g_BinOffset.Store(bin * 4u, runningOffset);

        // D3D12_DRAW_ARGUMENTS: VertexCountPerInstance, InstanceCount, StartVertexLocation, StartInstanceLocation
        uint argsBase = bin * 16u;
        g_IndirectArgs.Store(argsBase + 0u, count);       // VertexCountPerInstance = particle count in bin
        g_IndirectArgs.Store(argsBase + 4u, 1u);           // InstanceCount = 1
        g_IndirectArgs.Store(argsBase + 8u, runningOffset); // StartVertexLocation = bin offset
        g_IndirectArgs.Store(argsBase + 12u, 0u);          // StartInstanceLocation = 0

        // Zero bin counter for next frame (avoids separate clear pass)
        g_BinCounter.Store(bin * 4u, 0u);

        runningOffset += count;
    }
}
