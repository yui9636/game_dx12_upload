// ==========================================
// GTAOPS.hlsl
// ==========================================
#include "PBR.hlsli"

Texture2D NormalMap : register(t0);
Texture2D WorldPosMap : register(t1);

SamplerState PointSamp : register(s2);

struct VS_OUT_QUAD
{
    float4 pos : SV_POSITION;
    float2 uv : TEXCOORD;
};

float IGN(float2 pixelPos)
{
    return frac(52.9829189f * frac(dot(pixelPos, float2(0.06711056f, 0.00583715f))));
}

float4 main(VS_OUT_QUAD pin) : SV_TARGET
{
    float3 P = WorldPosMap.Sample(PointSamp, pin.uv).xyz;
    float3 N = NormalMap.Sample(PointSamp, pin.uv).xyz;

    if (length(N) < 0.1f)
        return float4(1.0f, 1.0f, 1.0f, 1.0f);
    N = normalize(N);

    float3 V_cam = P - cameraPosition.xyz;
    float distToCam = length(V_cam);
    if (distToCam < 0.1f)
        return float4(1.0f, 1.0f, 1.0f, 1.0f);
    V_cam /= distToCam;

    const int SLICES = 4;
    const int STEPS = 4;
    const float RADIUS = 1.0f;
    float ao = 0.0f;

    float temporalSeed = jitterX * 1337.0f + jitterY * 7331.0f;
    float noise = IGN(pin.pos.xy + temporalSeed);

    float3 right = cross(V_cam, float3(0.0f, 1.0f, 0.0f));
    if (length(right) < 0.001f)
        right = cross(V_cam, float3(1.0f, 0.0f, 0.0f));
    right = normalize(right);

    float4 clipCenter = mul(float4(P, 1.0f), viewProjectionUnjittered);
    float4 clipOffset = mul(float4(P + right * RADIUS, 1.0f), viewProjectionUnjittered);

    float2 ndcCenter = clipCenter.xy / clipCenter.w;
    float2 ndcOffset = clipOffset.xy / clipOffset.w;
    float radiusUV = length(ndcOffset - ndcCenter) * 0.5f;

    float aspectRatio = renderW / renderH;

    for (int i = 0; i < SLICES; ++i)
    {
        float phi = (i + noise) * (3.14159265f / SLICES);
        float2 dir = float2(cos(phi), sin(phi));
        dir.y *= aspectRatio;

        float maxAngle = -1.0f;

        for (int j = 1; j <= STEPS; ++j)
        {
            float2 offset = dir * (j / (float)STEPS) * radiusUV;
            float2 sampleUV = pin.uv + offset;

            if (sampleUV.x < 0.0f || sampleUV.x > 1.0f || sampleUV.y < 0.0f || sampleUV.y > 1.0f)
                continue;

            float3 samplePos = WorldPosMap.Sample(PointSamp, sampleUV).xyz;
            float3 V = samplePos - P;
            float dist = length(V);

            if (dist < 0.05f || dist > RADIUS)
                continue;

            V /= dist;
            float angle = dot(N, V);
            maxAngle = max(maxAngle, angle);
        }
        ao += max(0.0f, maxAngle);
    }

    ao = 1.0f - (ao / SLICES);
    return float4(ao, ao, ao, 1.0f);
}
