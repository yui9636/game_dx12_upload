// ScatterAlive: ページごとに生存パーティクル index を g_AliveList へ散布する。
// 各スレッドグループが 1 ページを担当し、グループ内スレッドがページ内 slot を走査する。
// ディスパッチ: (activePageCount, 1, 1)
// 共有 simulation root signature を使う: u0=AliveList
// Page table と header は SRV（t0, t1）で読む設計だが、共有 root signature を使うため、
// 実際にはバインド済みの UAV slot 経由で読む。
#include "EffectParticleRuntimeCommon.hlsli"
#include "EffectParticleSoA.hlsli"

// 共有 root signature では個別 SRV を簡単に使えない。
// そのため、ページ情報は simulation cbuffer から、header は UAV から読む。
// このシェーダは g_BillboardHeader（alive 確認）と g_AliveList（書き込み）を使う。
// バインド: u0=AliveList, u1=PageAliveOffset(read), u2=BillboardHeader(read)
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

    // 各スレッドはこのページ内の複数 slot を走査する。
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
