// ============================================================================
// DepthBinArgs: Read per-depth-bin counters, write D3D12_DRAW_ARGUMENTS per bin
// Also zero counters for next frame
// Dispatch: (1, 1, 1)
// ============================================================================

#include "EffectParticleSoA.hlsli"

#define DEPTH_BINS 32u

RWByteAddressBuffer g_DepthBinCounter : register(u0); // input: per-depth-bin counts
RWByteAddressBuffer g_IndirectArgs    : register(u1); // output: D3D12_DRAW_ARGUMENTS per depth bin (16B * 32)

[numthreads(1, 1, 1)]
void CSMain(uint3 dtid : SV_DispatchThreadID)
{
    uint runningOffset = 0u;

    for (uint bin = 0u; bin < DEPTH_BINS; ++bin)
    {
        uint count = g_DepthBinCounter.Load(bin * 4u);

        // D3D12_DRAW_ARGUMENTS per depth bin
        uint argsBase = bin * 16u;
        g_IndirectArgs.Store(argsBase + 0u, count);        // VertexCountPerInstance
        g_IndirectArgs.Store(argsBase + 4u, 1u);            // InstanceCount
        g_IndirectArgs.Store(argsBase + 8u, runningOffset);  // StartVertexLocation
        g_IndirectArgs.Store(argsBase + 12u, 0u);           // StartInstanceLocation

        // Zero counter for next frame
        g_DepthBinCounter.Store(bin * 4u, 0u);

        runningOffset += count;
    }
}
