// ============================================================================
// UpdateAlive: Read Hot + Cold, write Hot, side-write g_PageAliveCount
// Dispatch: (activeCount + 63) / 64
// Only processes alive particles (from alive list of previous frame)
// Register layout: t0=AliveListPrev, u0=Hot, u1=Warm, u2=Cold, u3=Header,
//   u4=DeadStack, u5=Counter, u6=RibbonHistory, u7=PageAliveCount
// ============================================================================

#include "EffectParticleRuntimeCommon.hlsli"
#include "EffectParticleSoA.hlsli"

StructuredBuffer<uint>              g_AliveListPrev    : register(t0);

RWStructuredBuffer<BillboardHot>    g_BillboardHot     : register(u0);
RWStructuredBuffer<BillboardWarm>   g_BillboardWarm    : register(u1);
RWStructuredBuffer<BillboardCold>   g_BillboardCold    : register(u2);
RWStructuredBuffer<BillboardHeader> g_BillboardHeader  : register(u3);
RWStructuredBuffer<uint>            g_DeadStack        : register(u4);
RWByteAddressBuffer                 g_CounterBuffer    : register(u5);
RWStructuredBuffer<float4>          g_RibbonHistory    : register(u6);
RWStructuredBuffer<uint>            g_PageAliveCount   : register(u7);

[numthreads(64, 1, 1)]
void CSMain(uint3 dispatchThreadId : SV_DispatchThreadID)
{
    const uint aliveIndex = dispatchThreadId.x;
    const uint activeCount = (uint)(gTiming.w + 0.5f);
    if (aliveIndex >= activeCount) return;

    const uint slot = g_AliveListPrev[aliveIndex];

    // ── Read Hot ──
    BillboardHot hot = g_BillboardHot[slot];
    float2 ageLife = UnpackHalf2(hot.ageLifePacked);
    float age = ageLife.x;
    float remainLife = ageLife.y;

    // Already dead check
    if (remainLife <= 0.0f) return;

    // ── Read Cold (immutable) ──
    BillboardCold cold = g_BillboardCold[slot];
    float2 dragSpin = UnpackHalf2(cold.dragSpinPacked);
    float2 sizeRange = UnpackHalf2(cold.sizeRange);
    float2 lifeBias = UnpackHalf2(cold.lifeBias);
    float2 sizeFadeBias = UnpackHalf2(cold.sizeFadeBias);

    const float totalLife = max(lifeBias.x, 0.001f);
    const float alphaBias = lifeBias.y;
    const float sizeBias = sizeFadeBias.x;
    const float drag = dragSpin.x;
    const float spinRate = dragSpin.y;
    const float startSize = sizeRange.x;
    const float endSize = sizeRange.y;

    const float dt = max(gTiming.x, 0.0f);

    // ── Age update ──
    age += dt;
    remainLife -= dt;

    if (remainLife <= 0.0f)
    {
        // Kill particle
        BillboardHeader hdr = {slot, 0u}; // alive=false
        g_BillboardHeader[slot] = hdr;

        // Push to dead stack
        uint deadIdx = 0u;
        g_CounterBuffer.InterlockedAdd(COUNTER_DEAD_STACK_TOP, 1u, deadIdx);
        g_DeadStack[deadIdx] = slot;

        // Zero out hot for clean render
        hot.ageLifePacked = PackHalf2(0.0f, 0.0f);
        hot.sizeSpin = PackHalf2(0.0f, 0.0f);
        g_BillboardHot[slot] = hot;
        return;
    }

    // ── Physics ──
    const float normalizedAge = saturate(age / totalLife);
    const float dragFactor = saturate(1.0f - drag * dt);

    hot.velocity += cold.acceleration * dt;

    // Curl noise
    const float curlNoiseStrength = max(gMotionParams.x, 0.0f);
    if (curlNoiseStrength > 0.001f)
    {
        const float3 noiseForce = SampleCurlNoiseVolume(hot.position, age);
        hot.velocity += noiseForce * curlNoiseStrength * dt;
    }

    // Vortex
    const float vortexStrength = gMotionParams.w;
    if (abs(vortexStrength) > 0.001f)
    {
        const float3 toParticle = hot.position - gOriginCurrentTime.xyz;
        const float distSq = max(dot(toParticle.xz, toParticle.xz), 0.001f);
        float3 tangent = normalize(float3(-toParticle.z, 0.0f, toParticle.x));
        hot.velocity += tangent * (vortexStrength / sqrt(distSq)) * dt;
    }

    hot.velocity *= dragFactor;
    hot.position += hot.velocity * dt;

    // ── Size + spin ──
    float2 curSizeSpin = UnpackHalf2(hot.sizeSpin);
    const float sizeAge = ComputeBiasedAge(normalizedAge, sizeBias);
    float currentSize = lerp(startSize, endSize, sizeAge);
    float currentSpin = curSizeSpin.y + spinRate * dt;

    hot.ageLifePacked = PackHalf2(age, remainLife);
    hot.sizeSpin = PackHalf2(currentSize, currentSpin);
    g_BillboardHot[slot] = hot;

    // ── Update Warm: color + sub-UV ──
    const float alphaAge = ComputeBiasedAge(normalizedAge, alphaBias);
    const float fade = saturate(1.0f - alphaAge);

    BillboardWarm warm = g_BillboardWarm[slot];
    float4 startColor = UnpackRGBA8(warm.packedColor);
    float4 endColor = UnpackRGBA8(warm.packedEndColor);
    float4 lifeTint = lerp(startColor, endColor, alphaAge);
    lifeTint.a *= fade;
    warm.packedColor = PackRGBA8(lifeTint);

    float4 subUv = ComputeSubUvRect(age, totalLife);
    warm.texcoordPacked = PackHalf2(subUv.x, subUv.y);
    g_BillboardWarm[slot] = warm;

    // ── Update Header depth bin ──
    BillboardHeader hdr;
    hdr.slotIndex = slot;
    const float3 camToP = hot.position - gCameraPositionSortSign.xyz;
    float viewZ = dot(camToP, gCameraDirectionCapacity.xyz);
    uint depthBin = ComputeDepthBin(viewZ, 0.1f, 1000.0f);
    hdr.packed = HeaderPack(true, depthBin, slot / PAGE_SIZE, 0u);
    g_BillboardHeader[slot] = hdr;

    // ── Ribbon history ──
    const uint historyBase = slot * EffectParticleRibbonHistoryLength;
    [unroll]
    for (uint h = EffectParticleRibbonHistoryLength - 1u; h > 0u; --h)
    {
        g_RibbonHistory[historyBase + h] = g_RibbonHistory[historyBase + h - 1u];
    }
    g_RibbonHistory[historyBase] = float4(hot.position, 1.0f);

    // ── Side-write: per-page alive count (for PrefixSum) ──
    uint pageIdx = slot / PAGE_SIZE;
    InterlockedAdd(g_PageAliveCount[pageIdx], 1u);
}
