// Emit: dead stack から pop し、Hot/Warm/Cold の SoA stream と Header へ書く。
// ディスパッチ: CPU から (emitCount, 1, 1)
#include "EffectParticleRuntimeCommon.hlsli"
#include "EffectParticleSoA.hlsli"

RWStructuredBuffer<BillboardHot>    g_BillboardHot    : register(u0);
RWStructuredBuffer<BillboardWarm>   g_BillboardWarm   : register(u1);
RWStructuredBuffer<BillboardCold>   g_BillboardCold   : register(u2);
RWStructuredBuffer<BillboardHeader> g_BillboardHeader  : register(u3);
RWStructuredBuffer<uint>            g_DeadStack        : register(u4);
RWByteAddressBuffer                 g_CounterBuffer    : register(u5);
RWStructuredBuffer<float4>          g_RibbonHistory    : register(u6);
RWStructuredBuffer<uint>            g_PageAliveCount   : register(u7);
RWStructuredBuffer<MeshAttribHot>   g_MeshAttribHot    : register(u8);

[numthreads(64, 1, 1)]
void CSMain(uint3 dispatchThreadId : SV_DispatchThreadID)
{
    const uint emitIndex = dispatchThreadId.x;
    const uint emitCount = (uint)(gTiming.w + 0.5f);
    if (emitIndex >= emitCount) return;

    // dead stack から pop する（atomic decrement）。
    uint oldDeadCount = 0u;
    g_CounterBuffer.InterlockedAdd(COUNTER_DEAD_STACK_TOP, 0xFFFFFFFFu, oldDeadCount);
    if (oldDeadCount == 0u)
    {
        g_CounterBuffer.InterlockedAdd(COUNTER_DEAD_STACK_TOP, 1u);
        g_CounterBuffer.InterlockedAdd(COUNTER_OVERFLOW, 1u);
        return;
    }

    const uint slot = g_DeadStack[oldDeadCount - 1u];

    // cbuffer パラメータを展開する。
    const uint shapeType = (uint)(gShapeTypeSpinAlphaBias.x + 0.5f);
    const uint seed = (uint)(gSizeSeed.z + 0.5f);
    const float spinRate = gShapeTypeSpinAlphaBias.y;
    const float speed = max(gTiming.z, 0.0f);
    const float particleLifetime = max(gTiming.y, 0.001f);
    const float3 acceleration = gAccelerationDrag.xyz;
    const float drag = max(gAccelerationDrag.w, 0.0f);
    const float startSize = gSizeSeed.x;
    const float endSize = gSizeSeed.y;
    const float sizeBias = gShapeParametersSizeBias.w;
    const float alphaBias = gShapeTypeSpinAlphaBias.z;

    const float randomSpeedRange = gRandomParams.x;
    const float randomSizeRange = gRandomParams.y;
    const float randomLifeRange = gRandomParams.z;

    float3 direction = float3(0.0f, 1.0f, 0.0f);
    const float3 spawnOffset = ComputeSpawnOffset(emitIndex, seed + slot * 13u, shapeType, gShapeParametersSizeBias.xyz, direction);
    const float baseSpeedRand = lerp(0.65f, 1.35f, Hash01(seed * 3571u + slot * 2137u + emitIndex * 7417u + 131u));
    const float randSpin = lerp(-spinRate, spinRate, Hash01(seed * 1597u + slot * 6481u + emitIndex * 3253u + 97u));

    // slot hash からパーティクルごとの乱数係数を作る。
    const float randSpeedFactor = lerp(1.0f - randomSpeedRange, 1.0f + randomSpeedRange, Hash01(seed * 4219u + slot * 8837u + 271u));
    const float randSizeFactor = lerp(1.0f - randomSizeRange, 1.0f + randomSizeRange, Hash01(seed * 5387u + slot * 7193u + 353u));
    const float randLifeFactor = lerp(1.0f - randomLifeRange, 1.0f + randomLifeRange, Hash01(seed * 6571u + slot * 3911u + 479u));

    const float finalSpeed = speed * baseSpeedRand * randSpeedFactor;
    const float finalLife = max(particleLifetime * randLifeFactor, 0.01f);
    const float finalStartSize = startSize * randSizeFactor;
    const float finalEndSize = endSize * randSizeFactor;

    // 32B の Hot stream へ書き込む。
    BillboardHot hot;
    hot.position = gOriginCurrentTime.xyz + spawnOffset;
    hot.ageLifePacked = PackHalf2(0.0f, finalLife);
    hot.velocity = direction * finalSpeed;
    hot.sizeSpin = PackHalf2(finalStartSize, 0.0f);
    g_BillboardHot[slot] = hot;

    // 16B の Warm stream へ書き込む。
    BillboardWarm warm;
    warm.packedColor = PackRGBA8(gTint);
    warm.packedEndColor = PackRGBA8(gTintEnd);
    warm.texcoordPacked = PackHalf2(0.0f, 0.0f); // sub-UV は update 側で計算する。
    warm.flags = 0u; // TODO: emitter パラメータから blendMode / sortMode を設定する。
    g_BillboardWarm[slot] = warm;

    // emit 後は不変の 32B Cold stream へ書き込む。
    BillboardCold cold;
    cold.acceleration = acceleration;
    cold.dragSpinPacked = PackHalf2(drag, randSpin);
    cold.sizeRange = PackHalf2(finalStartSize, finalEndSize);
    cold.lifeBias = PackHalf2(finalLife, alphaBias);
    cold.sizeFadeBias = PackHalf2(sizeBias, 0.0f); // fadeBias は予約領域
    cold.emitterSeed = seed;
    g_BillboardCold[slot] = cold;

    // 8B の Header へ書き込む。
    BillboardHeader hdr;
    hdr.slotIndex = slot;
    hdr.packed = HeaderPack(true, 0u, slot / PAGE_SIZE, 0u);
    g_BillboardHeader[slot] = hdr;

    // PrefixSum 用に page ごとの alive count を横書きする。
    InterlockedAdd(g_PageAliveCount[slot / PAGE_SIZE], 1u);

    // ribbon 履歴を初期化する。
    const float4 posW = float4(hot.position, 1.0f);
    const uint historyBase = slot * EffectParticleRibbonHistoryLength;
    [unroll]
    for (uint h = 0u; h < EffectParticleRibbonHistoryLength; ++h)
    {
        g_RibbonHistory[historyBase + h] = posW;
    }

    // Mesh renderer bin のときだけ mesh attribute を初期化する。
    // gMeshFlags.x: 0=ビルボード/リボン（スキップ）、1=メッシュ
    if (gMeshFlags.x > 0.5f)
    {
        // 基準回転軸と速度に、パーティクルごとの yaw / pitch / roll 乱数オフセットを足す。
        const float yawRand   = lerp(-gMeshAngularRandomOrient.x, gMeshAngularRandomOrient.x, Hash01(seed * 7919u + slot * 1213u + 61u));
        const float pitchRand = lerp(-gMeshAngularRandomOrient.y, gMeshAngularRandomOrient.y, Hash01(seed * 2683u + slot * 9973u + 139u));
        const float rollRand  = lerp(-gMeshAngularRandomOrient.z, gMeshAngularRandomOrient.z, Hash01(seed * 4093u + slot * 2437u + 239u));
        const float4 initialRotation = QuatNormalize(QuatFromYawPitchRoll(yawRand, pitchRand, rollRand));

        // スケール: base xyz * (1 + random)。パーティクルごとに一様乱数を掛け、縦横比は保つ。
        const float scaleRandRange = max(gMeshInitialScale.w, 0.0f);
        const float scaleRandFactor = lerp(1.0f - scaleRandRange, 1.0f + scaleRandRange, Hash01(seed * 8527u + slot * 5479u + 317u));
        const float3 finalScale = gMeshInitialScale.xyz * scaleRandFactor;

        // 角速度: base rad/s * (1 + random)。
        const float speedRandRange = max(gMeshAngularRandomOrient.w, 0.0f);
        const float speedRandFactor = lerp(1.0f - speedRandRange, 1.0f + speedRandRange, Hash01(seed * 3697u + slot * 6113u + 409u));
        const float finalAngularSpeed = gMeshAngularAxisSpeed.w * speedRandFactor;

        // 軸を正規化する（cbuffer には未正規化入力が入ることがある）。
        const float3 axisIn = gMeshAngularAxisSpeed.xyz;
        const float axisLen = length(axisIn);
        const float3 finalAxis = axisLen > 1e-5f ? (axisIn / axisLen) : float3(0.0f, 1.0f, 0.0f);

        MeshAttribHot mattr;
        mattr.rotation      = initialRotation;
        mattr.scale         = finalScale;
        mattr.angularSpeed  = finalAngularSpeed;
        mattr.angularAxis   = finalAxis;
        mattr.rotReserved   = 0.0f;
        mattr.reserved      = uint4(0u, 0u, 0u, 0u);
        g_MeshAttribHot[slot] = mattr;
    }
}
