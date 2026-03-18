// ==========================================
// VolumetricFogBlurPS.hlsl
// ==========================================
#include "FullScreenQuad.hlsli"

Texture2D FogMap : register(t0);
Texture2D GBuffer2 : register(t1);

SamplerState pointSampler : register(s2);

float4 main(VS_OUT pin) : SV_TARGET
{
    uint width, height;
    FogMap.GetDimensions(width, height);
    float2 texelSize = 1.0f / float2(width, height);

    float3 centerPos = GBuffer2.Sample(pointSampler, pin.texcoord).xyz;
    float centerDepth = GBuffer2.Sample(pointSampler, pin.texcoord).w;

    float3 totalFog = float3(0.0f, 0.0f, 0.0f);
    float totalWeight = 0.0f;
    const int BLUR_RADIUS = 3;

    [unroll]
    for (int y = -BLUR_RADIUS; y <= BLUR_RADIUS; ++y)
    {
        [unroll]
        for (int x = -BLUR_RADIUS; x <= BLUR_RADIUS; ++x)
        {
            float2 offset = float2(x, y) * texelSize;
            float2 sampleUV = pin.texcoord + offset;

            float3 sampleFog = FogMap.SampleLevel(pointSampler, sampleUV, 0).rgb;
            float3 samplePos = GBuffer2.SampleLevel(pointSampler, sampleUV, 0).xyz;
            float sampleDepth = GBuffer2.SampleLevel(pointSampler, sampleUV, 0).w;

            float spatialWeight = exp(-(x * x + y * y) / 8.0f);
            float depthWeight = 1.0f;
            if (centerDepth < 1.0f && sampleDepth < 1.0f)
            {
                float posDiff = length(centerPos - samplePos);
                depthWeight = exp(-posDiff * 2.0f);
            }
            else if (centerDepth >= 1.0f && sampleDepth >= 1.0f)
            {
                depthWeight = 1.0f;
            }
            else
            {
                depthWeight = 0.0f;
            }

            float weight = spatialWeight * depthWeight;
            totalFog += sampleFog * weight;
            totalWeight += weight;
        }
    }

    return float4(totalFog / max(totalWeight, 0.0001f), 1.0f);
}
