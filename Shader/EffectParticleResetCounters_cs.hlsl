// ResetCounters: フレームごとの counter と page alive count を 0 にする。
// ディスパッチ: (totalPages + 63) / 64
// 共有 simulation root signature を使う。totalPages は gTiming.w で渡す。
#include "EffectParticleRuntimeCommon.hlsli"
#include "EffectParticleSoA.hlsli"

// 共有 root signature: u0=Hot, u1=Warm, ...
// ここでは u0=Counter、u1=PageAliveCount として流用する（必要なのはこの 2 つだけ）。
RWByteAddressBuffer g_CounterBuffer      : register(u0);
RWStructuredBuffer<uint> g_PageAliveCount : register(u1);

[numthreads(64, 1, 1)]
void CSMain(uint3 dtid : SV_DispatchThreadID)
{
    const uint totalPages = (uint)(gTiming.w + 0.5f);

    // スレッド 0: グローバル counter をリセットする。
    if (dtid.x == 0u)
    {
        g_CounterBuffer.Store(COUNTER_ALIVE_BILLBOARD, 0u);
        g_CounterBuffer.Store(COUNTER_ALIVE_MESH, 0u);
        g_CounterBuffer.Store(COUNTER_ALIVE_RIBBON, 0u);
        g_CounterBuffer.Store(COUNTER_ALIVE_TOTAL, 0u);
        g_CounterBuffer.Store(COUNTER_OVERFLOW, 0u);
        g_CounterBuffer.Store(COUNTER_DROPPED_EMIT, 0u);
    }

    // ページごとの alive count を 0 にする。
    if (dtid.x < totalPages)
    {
        g_PageAliveCount[dtid.x] = 0u;
    }
}
