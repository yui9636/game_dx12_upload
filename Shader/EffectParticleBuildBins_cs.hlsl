// BuildRendererBins: 2 段階のグループローカル bin 分け。
// 段階 1: スレッドグループ内でローカル bin 数を集計する。
// 段階 2: グループごとに各 bin へ 1 回だけグローバル atomic を行う（N 回ではなく N/256 回）。
// 段階 3: パーティクル index を bin ソート済み位置へ散布する。
// ディスパッチ: ((aliveCount + 255) / 256, 1, 1)
#include "EffectParticleRuntimeCommon.hlsli"
#include "EffectParticleSoA.hlsli"

// MAX_BINS は blendMode(2bit) と sortMode(2bit) から実用上 16 bin とする。
// groupshared を単純化するため 2 の累乗を保つ。
#define MAX_BINS 16u
#define GROUP_SIZE 256u

StructuredBuffer<uint>            g_AliveList       : register(t0);
StructuredBuffer<BillboardWarm>   g_BillboardWarm   : register(t1);
StructuredBuffer<BillboardHeader> g_BillboardHeader : register(t2);

RWStructuredBuffer<uint>  g_BinIndex   : register(u0); // 出力: bin ソート済みパーティクル index
RWByteAddressBuffer       g_BinCounter : register(u1); // bin ごとの global counter。4B * MAX_BINS。
RWByteAddressBuffer       g_CounterBuffer : register(u2);

groupshared uint s_localBinCount[MAX_BINS];
groupshared uint s_localBinOffset[MAX_BINS]; // このグループチャンクのグローバルオフセット
groupshared uint s_localPrefix[MAX_BINS];    // グループ内の排他的 prefix

// Warm flags から bin key を取り出す。
// flags レイアウト: blendMode(2) | sortMode(2) | material(8) | subUvFrame(8) | soft(1) | ...
// BinKey = blendMode(2) | sortMode(2)。flags の [0:3] bit を使う。
uint ExtractBinKey(uint flags)
{
    return flags & 0xFu; // 下位 4 bit: blendMode(2) + sortMode(2)
}

[numthreads(GROUP_SIZE, 1, 1)]
void CSMain(uint3 dtid : SV_DispatchThreadID, uint3 gid : SV_GroupID, uint gtid : SV_GroupIndex)
{
    // 生存数を取得する。
    uint aliveCount = g_CounterBuffer.Load(COUNTER_ALIVE_BILLBOARD);

    // Stage 0: ローカル bin を 0 で初期化する。
    if (gtid < MAX_BINS)
    {
        s_localBinCount[gtid] = 0u;
    }
    GroupMemoryBarrierWithGroupSync();

    // Stage 1: 分類し、スレッドグループ内で集計する。
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

    // Stage 2: bin ごとに 1 スレッドだけ global atomic を行う。
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

        // scatter 用のローカル排他的 prefix sum を計算する。
        // MAX_BINS は小さい（16）ので単純な直列 scan で十分。
        uint prefix = 0u;
        for (uint b = 0u; b < gtid; ++b)
        {
            prefix += s_localBinCount[b];
        }
        s_localPrefix[gtid] = prefix;
    }
    GroupMemoryBarrierWithGroupSync();

    // Stage 3: bin sort 済み位置へ散布する。
    // 各スレッドが、このグループ内における自身の bin 内位置を知る必要がある。
    // 各スレッドの bin 内 local offset を得るため、local atomic をもう一度行う。
    // 先にローカルカウントをリセットする。
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
