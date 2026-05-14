// Depth 用。BinArgs: depth bin ごとの counter を読み、bin ごとの D3D12_DRAW_ARGUMENTS を書く。
// 次フレーム用に counter も 0 にする。
// ディスパッチ: (1, 1, 1)
#include "EffectParticleSoA.hlsli"

#define DEPTH_BINS 32u

RWByteAddressBuffer g_DepthBinCounter : register(u0); // 入力: depth bin ごとの数
RWByteAddressBuffer g_IndirectArgs    : register(u1); // 出力: depth bin ごとの D3D12_DRAW_ARGUMENTS（16B * 32）

[numthreads(1, 1, 1)]
void CSMain(uint3 dtid : SV_DispatchThreadID)
{
    uint runningOffset = 0u;

    for (uint bin = 0u; bin < DEPTH_BINS; ++bin)
    {
        uint count = g_DepthBinCounter.Load(bin * 4u);

        // depth bin ごとの D3D12_DRAW_ARGUMENTS を書き込む。
        uint argsBase = bin * 16u;
        g_IndirectArgs.Store(argsBase + 0u, count);        // VertexCountPerInstance を書き込む。
        g_IndirectArgs.Store(argsBase + 4u, 1u);            // InstanceCount を書き込む。
        g_IndirectArgs.Store(argsBase + 8u, runningOffset);  // StartVertexLocation を書き込む。
        g_IndirectArgs.Store(argsBase + 12u, 0u);           // StartInstanceLocation を書き込む。

        // 次フレーム用に counter を 0 にする。
        g_DepthBinCounter.Store(bin * 4u, 0u);

        runningOffset += count;
    }
}
