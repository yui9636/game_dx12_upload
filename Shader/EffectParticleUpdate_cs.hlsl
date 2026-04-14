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

    // Wind
    const float windStrength = gRandomParams.w;
    if (windStrength > 0.001f)
    {
        float3 windDir = normalize(gWindDirection.xyz + float3(0.001f, 0.0f, 0.0f));
        const float turbulence = gWindDirection.w;
        if (turbulence > 0.001f)
        {
            float3 noise = SamplePseudoNoise3(hot.position * 2.0f + float3(age * 0.7f, 0.0f, 0.0f));
            windDir += noise * turbulence;
            windDir = normalize(windDir);
        }
        hot.velocity += windDir * windStrength * dt;
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

    // Phase 2: Attractor/Repeller
    const uint attractorCount = (uint)(gCollisionParams.w + 0.5f);
    if (attractorCount > 0u)
    {
        float4 attractors[4] = { gAttractor0, gAttractor1, gAttractor2, gAttractor3 };
        [unroll]
        for (uint ai = 0; ai < 4; ++ai)
        {
            if (ai >= attractorCount) break;
            float3 aPos = attractors[ai].xyz;
            float aStr = attractors[ai].w;
            float4 _radii = gAttractorRadii;
            float4 _fall  = gAttractorFalloff;
            float aRad  = max(0.01f, ai == 0 ? _radii.x : ai == 1 ? _radii.y : ai == 2 ? _radii.z : _radii.w);
            float aFall = ai == 0 ? _fall.x : ai == 1 ? _fall.y : ai == 2 ? _fall.z : _fall.w;

            float3 toAttractor = aPos - hot.position;
            float dist = length(toAttractor);
            if (dist < 0.001f) continue;

            float3 dir = toAttractor / dist;
            float normDist = saturate(dist / aRad);
            float attenuation = 1.0f;
            if (aFall >= 1.5f) attenuation = 1.0f - normDist * normDist;
            else if (aFall >= 0.5f) attenuation = 1.0f - normDist;

            hot.velocity += dir * aStr * max(attenuation, 0.0f) * dt;
        }
    }

    hot.velocity *= dragFactor;
    hot.position += hot.velocity * dt;

    // Phase 2: GPU Collision
    const bool collisionEnabled = (gCollisionParams.z > 0.0f || dot(gCollisionPlane.xyz, gCollisionPlane.xyz) > 0.5f);
    if (collisionEnabled)
    {
        const float restitution = gCollisionParams.x;
        const float friction = gCollisionParams.y;
        const uint sphereCount = (uint)(gCollisionParams.z + 0.5f);

        // Plane collision
        float3 planeN = normalize(gCollisionPlane.xyz);
        float planeD = gCollisionPlane.w;
        float planeDist = dot(hot.position, planeN) + planeD;
        if (planeDist < 0.0f)
        {
            hot.position -= planeN * planeDist;
            float vn = dot(hot.velocity, planeN);
            if (vn < 0.0f)
            {
                float3 vNormal = planeN * vn;
                float3 vTangent = hot.velocity - vNormal;
                hot.velocity = -vNormal * restitution + vTangent * (1.0f - friction);
            }
        }

        // Sphere collision
        float4 spheres[4] = { gCollisionSphere0, gCollisionSphere1, gCollisionSphere2, gCollisionSphere3 };
        [unroll]
        for (uint si = 0; si < 4; ++si)
        {
            if (si >= sphereCount) break;
            float3 sCenter = spheres[si].xyz;
            float sRadius = spheres[si].w;
            if (sRadius <= 0.0f) continue;

            float3 toParticle2 = hot.position - sCenter;
            float dist2 = length(toParticle2);
            if (dist2 < sRadius && dist2 > 0.001f)
            {
                float3 normal = toParticle2 / dist2;
                hot.position = sCenter + normal * sRadius;
                float vn2 = dot(hot.velocity, normal);
                if (vn2 < 0.0f)
                {
                    float3 vNormal2 = normal * vn2;
                    float3 vTangent2 = hot.velocity - vNormal2;
                    hot.velocity = -vNormal2 * restitution + vTangent2 * (1.0f - friction);
                }
            }
        }
    }

    // ── Size + spin ──
    float2 curSizeSpin = UnpackHalf2(hot.sizeSpin);
    const float sizeAge = ComputeBiasedAge(normalizedAge, sizeBias);
    // Phase 1C: Use 4-key size curve if available, else legacy lerp
    float currentSize;
    if (gSizeCurveTimes.y > 0.001f || gSizeCurveTimes.z < 0.999f) {
        currentSize = EvaluateSizeCurve(sizeAge);
    } else {
        currentSize = lerp(startSize, endSize, sizeAge);
    }
    float currentSpin = curSizeSpin.y + spinRate * dt;

    hot.ageLifePacked = PackHalf2(age, remainLife);
    hot.sizeSpin = PackHalf2(currentSize, currentSpin);
    g_BillboardHot[slot] = hot;

    // ── Update Warm: color + sub-UV ──
    const float alphaAge = ComputeBiasedAge(normalizedAge, alphaBias);
    const float fade = saturate(1.0f - alphaAge);

    BillboardWarm warm = g_BillboardWarm[slot];
    // Phase 1C: Use 4-key color gradient if available, else legacy 2-key lerp
    float4 lifeTint;
    if (gGradientTimes.y > 0.001f || gGradientTimes.z < 0.999f) {
        lifeTint = EvaluateColorGradient(alphaAge);
    } else {
        float4 startColor = UnpackRGBA8(warm.packedColor);
        float4 endColor = UnpackRGBA8(warm.packedEndColor);
        lifeTint = lerp(startColor, endColor, alphaAge);
    }
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
