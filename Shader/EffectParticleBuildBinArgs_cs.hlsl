// BuildBinArgs: bin ごとのカウンタを読み、bin ごとの D3D12_DRAW_ARGUMENTS を書く。
// 描画用の bin offset（排他的 prefix sum）も計算する。
// ディスパッチ: (1, 1, 1)
#include "EffectParticleSoA.hlsli"

#define MAX_BINS 16u

RWByteAddressBuffer g_BinCounter   : register(u0); // 入力: bin ごとの数（4B * MAX_BINS）
RWByteAddressBuffer g_IndirectArgs : register(u1); // 出力: bin ごとの D3D12_DRAW_ARGUMENTS（16B * MAX_BINS）
RWByteAddressBuffer g_BinOffset    : register(u2); // 出力: bin ごとの排他的 prefix sum（4B * MAX_BINS）

[numthreads(1, 1, 1)]
void CSMain(uint3 dtid : SV_DispatchThreadID)
{
    // MAX_BINS は小さいため、単一スレッドの直列 prefix sum で十分。
    uint runningOffset = 0u;

    for (uint bin = 0u; bin < MAX_BINS; ++bin)
    {
        uint count = g_BinCounter.Load(bin * 4u);

        // 排他的 prefix を書く（g_BinIndex 内でのこの bin の開始 offset）。
        g_BinOffset.Store(bin * 4u, runningOffset);

        // D3D12_DRAW_ARGUMENTS は VertexCountPerInstance、InstanceCount、StartVertexLocation、StartInstanceLocation の順で格納する。
        uint argsBase = bin * 16u;
        g_IndirectArgs.Store(argsBase + 0u, count);       // VertexCountPerInstance を書き込む。 = bin 内のパーティクル数
        g_IndirectArgs.Store(argsBase + 4u, 1u);           // InstanceCount は 1 固定。
        g_IndirectArgs.Store(argsBase + 8u, runningOffset); // StartVertexLocation には bin offset を入れる。
        g_IndirectArgs.Store(argsBase + 12u, 0u);          // StartInstanceLocation は 0 固定。

        // 次フレーム用に bin counter を 0 にする（別 clear pass を避ける）。
        g_BinCounter.Store(bin * 4u, 0u);

        runningOffset += count;
    }
}
