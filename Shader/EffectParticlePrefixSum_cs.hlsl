// g_PageAliveCount から g_PageAliveOffset への PrefixSum。
// 効率重視の Blelloch scan（排他的 prefix sum）。
// ディスパッチ: (1, 1, 1)。単一グループで最大 2048 ページを処理する。
// 共有 simulation root signature を使う: b0=CBV, u0=PageAliveCount, u1=PageAliveOffset, u2=CounterBuffer
// activePageCount は makeSimulationConstants 経由で gTiming.w に渡す。
#include "EffectParticleRuntimeCommon.hlsli"
#include "EffectParticleSoA.hlsli"

RWStructuredBuffer<uint>       g_PageAliveCount   : register(u0);
RWStructuredBuffer<uint>       g_PageAliveOffset  : register(u1);
RWByteAddressBuffer            g_CounterBuffer    : register(u2);

#define GROUP_SIZE 1024
#define MAX_PAGES  2048  // 最大 2048 ページ、約 1600 万パーティクルまで対応

groupshared uint s_data[MAX_PAGES];

[numthreads(GROUP_SIZE, 1, 1)]
void CSMain(uint3 dtid : SV_DispatchThreadID, uint gtid : SV_GroupIndex)
{
    const uint activePageCount = (uint)(gTiming.w + 0.5f);

    // 読み込み段階: 各スレッドが 2 要素を読み込む。
    uint ai = gtid;
    uint bi = gtid + GROUP_SIZE;

    s_data[ai] = (ai < activePageCount) ? g_PageAliveCount[ai] : 0u;
    s_data[bi] = (bi < activePageCount) ? g_PageAliveCount[bi] : 0u;

    // up-sweep で部分和を畳み込む。
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

    // 総 alive 数を保存し、その後で最後の要素を 0 にする。
    if (gtid == 0u)
    {
        uint totalAlive = s_data[MAX_PAGES - 1u];
        g_CounterBuffer.Store(COUNTER_ALIVE_BILLBOARD, totalAlive);
        s_data[MAX_PAGES - 1u] = 0u;
    }

    // ダウンスイープ
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

    // 結果を書き込む。
    if (ai < activePageCount) g_PageAliveOffset[ai] = s_data[ai];
    if (bi < activePageCount) g_PageAliveOffset[bi] = s_data[bi];
}
