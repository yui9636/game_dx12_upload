// CoarseDepthBin: alpha / premul ビルボード向けの 2 段階グループローカル depth binning。
// 32 個の depth bin を log2 間隔で使い、奥から手前の順に描画する。
// ディスパッチ: ((alphaAliveCount + 255) / 256, 1, 1)
#include "EffectParticleRuntimeCommon.hlsli"
#include "EffectParticleSoA.hlsli"

#define DEPTH_BINS 32u
#define GROUP_SIZE 256u

StructuredBuffer<uint>          g_AliveList     : register(t0);
StructuredBuffer<BillboardHot>  g_BillboardHot  : register(t1);

RWStructuredBuffer<uint>  g_DepthBinIndex   : register(u0); // 出力: depth ソート済みパーティクル index
RWByteAddressBuffer       g_DepthBinCounter : register(u1); // depth bin ごとの global counter。4B * 32。
RWByteAddressBuffer       g_CounterBuffer   : register(u2); // alive count 用

cbuffer CoarseDepthParams : register(b0)
{
    float4x4 gViewMatrix;       // world から view への変換
    float    gNearClip;
    float    gFarClip;
    uint     gAliveCount;
    uint     gPad;
};

groupshared uint s_localDepthCount[DEPTH_BINS];
groupshared uint s_localDepthOffset[DEPTH_BINS]; // このグループチャンクのグローバルオフセット

[numthreads(GROUP_SIZE, 1, 1)]
void CSMain(uint3 dtid : SV_DispatchThreadID, uint3 gid : SV_GroupID, uint gtid : SV_GroupIndex)
{
    // Stage 0: ローカル bin を 0 で初期化する。
    if (gtid < DEPTH_BINS)
    {
        s_localDepthCount[gtid] = 0u;
    }
    GroupMemoryBarrierWithGroupSync();

    // Stage 1: 分類し、スレッドグループ内で集計する。
    uint depthBin = 0u;
    bool valid = dtid.x < gAliveCount;
    if (valid)
    {
        uint particleIdx = g_AliveList[dtid.x];
        float3 worldPos = g_BillboardHot[particleIdx].position;
        // view-Z 深度を得るため view 空間へ変換する。
        float viewZ = dot(float4(worldPos, 1.0f), gViewMatrix[2]);
        viewZ = abs(viewZ); // 正値にそろえる。
        depthBin = ComputeDepthBin(viewZ, gNearClip, gFarClip);

        uint localIdx;
        InterlockedAdd(s_localDepthCount[depthBin], 1u, localIdx);
    }
    GroupMemoryBarrierWithGroupSync();

    // Stage 2: bin ごとに 1 スレッドだけ global atomic を行う。
    if (gtid < DEPTH_BINS)
    {
        uint count = s_localDepthCount[gtid];
        if (count > 0u)
        {
            uint globalOffset;
            g_DepthBinCounter.InterlockedAdd(gtid * 4u, count, globalOffset);
            s_localDepthOffset[gtid] = globalOffset;
        }
        else
        {
            s_localDepthOffset[gtid] = 0u;
        }
    }
    GroupMemoryBarrierWithGroupSync();

    // Stage 3: depth sort 済み位置へ散布する。
    if (gtid < DEPTH_BINS)
    {
        s_localDepthCount[gtid] = 0u;
    }
    GroupMemoryBarrierWithGroupSync();

    if (valid)
    {
        uint particleIdx = g_AliveList[dtid.x];
        float3 worldPos = g_BillboardHot[particleIdx].position;
        float viewZ = abs(dot(float4(worldPos, 1.0f), gViewMatrix[2]));
        depthBin = ComputeDepthBin(viewZ, gNearClip, gFarClip);

        uint localSlot;
        InterlockedAdd(s_localDepthCount[depthBin], 1u, localSlot);

        uint globalPos = s_localDepthOffset[depthBin] + localSlot;
        g_DepthBinIndex[globalPos] = particleIdx;
    }
}
