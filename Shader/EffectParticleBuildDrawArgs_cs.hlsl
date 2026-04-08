// ============================================================================
// BuildDrawArgs: Reads totalAliveBillboard from counter buffer,
// writes D3D12_DRAW_ARGUMENTS for ExecuteIndirect
// Dispatch: (1, 1, 1)
// ============================================================================

#include "EffectParticleSoA.hlsli"

RWByteAddressBuffer g_IndirectArgs  : register(u0);
RWByteAddressBuffer g_CounterBuffer : register(u1);

[numthreads(1, 1, 1)]
void CSMain(uint3 dtid : SV_DispatchThreadID)
{
    if (dtid.x != 0u) return;

    uint aliveCount = g_CounterBuffer.Load(COUNTER_ALIVE_BILLBOARD);

    // D3D12_DRAW_ARGUMENTS: VertexCountPerInstance, InstanceCount, StartVertexLocation, StartInstanceLocation
    g_IndirectArgs.Store(0u, aliveCount);
    g_IndirectArgs.Store(4u, 1u);
    g_IndirectArgs.Store(8u, 0u);
    g_IndirectArgs.Store(12u, 0u);
}
