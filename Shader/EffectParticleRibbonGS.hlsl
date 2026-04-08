// ============================================================================
// Ribbon GS (SoA): Reads AliveList -> Hot + Warm + RibbonHistory
// Expands point to ribbon strip segments
// ============================================================================

#include "compute_particle.hlsli"
#include "EffectParticleSoA.hlsli"

StructuredBuffer<uint>           g_AliveList      : register(t0);
StructuredBuffer<BillboardHot>   g_BillboardHot   : register(t1);
StructuredBuffer<BillboardWarm>  g_BillboardWarm  : register(t2);
StructuredBuffer<float4>         g_RibbonHistory  : register(t3);

static const uint RibbonHistoryLength = 8;

float3 SafeNormalize(float3 value, float3 fallbackValue)
{
    const float lenSq = dot(value, value);
    if (lenSq > 1.0e-6f) {
        return value * rsqrt(lenSq);
    }
    return fallbackValue;
}

[maxvertexcount((RibbonHistoryLength - 1) * 4)]
void main(point GS_IN gin[1], inout TriangleStream<PS_IN> output)
{
    const uint aliveIndex = gin[0].vertex_id;
    const uint slot = g_AliveList[aliveIndex];

    // Read SoA streams
    BillboardHot hot = g_BillboardHot[slot];
    BillboardWarm warm = g_BillboardWarm[slot];

    float2 sizeSpin = UnpackHalf2(hot.sizeSpin);
    float currentSize = sizeSpin.x;
    if (currentSize <= 0.0f) return;

    float4 color = UnpackRGBA8(warm.packedColor);
    float halfWidth = currentSize * 0.5f;

    [unroll]
    for (uint segmentIndex = 0u; segmentIndex < RibbonHistoryLength - 1u; ++segmentIndex) {
        const float3 p0 = g_RibbonHistory[slot * RibbonHistoryLength + segmentIndex].xyz;
        const float3 p1 = g_RibbonHistory[slot * RibbonHistoryLength + segmentIndex + 1u].xyz;
        const float3 segment = p1 - p0;
        if (dot(segment, segment) < 1.0e-6f) {
            continue;
        }

        const float3 midpoint = (p0 + p1) * 0.5f;
        const float3 viewDir = SafeNormalize(cameraPosition.xyz - midpoint, float3(0.0f, 0.0f, 1.0f));
        const float3 segmentDir = SafeNormalize(segment, float3(0.0f, 1.0f, 0.0f));
        float3 side = cross(viewDir, segmentDir);
        side = SafeNormalize(side, float3(1.0f, 0.0f, 0.0f));

        const float headT = (float)segmentIndex / (float)(RibbonHistoryLength - 1u);
        const float tailT = (float)(segmentIndex + 1u) / (float)(RibbonHistoryLength - 1u);
        const float headAlpha = saturate((1.0f - headT) * global_alpha) * color.a;
        const float tailAlpha = saturate((1.0f - tailT) * global_alpha) * color.a;

        const float3 offset = side * halfWidth;

        PS_IN verts[4];
        verts[0].position = mul(float4(p0 - offset, 1.0f), viewProjection);
        verts[0].texcoord = float2(headT, 0.0f);
        verts[0].color = float4(color.rgb, headAlpha);

        verts[1].position = mul(float4(p0 + offset, 1.0f), viewProjection);
        verts[1].texcoord = float2(headT, 1.0f);
        verts[1].color = float4(color.rgb, headAlpha);

        verts[2].position = mul(float4(p1 - offset, 1.0f), viewProjection);
        verts[2].texcoord = float2(tailT, 0.0f);
        verts[2].color = float4(color.rgb, tailAlpha);

        verts[3].position = mul(float4(p1 + offset, 1.0f), viewProjection);
        verts[3].texcoord = float2(tailT, 1.0f);
        verts[3].color = float4(color.rgb, tailAlpha);

        output.Append(verts[0]);
        output.Append(verts[1]);
        output.Append(verts[2]);
        output.Append(verts[3]);
        output.RestartStrip();
    }
}
