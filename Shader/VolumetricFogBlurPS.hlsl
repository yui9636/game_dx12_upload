// ==========================================
// VolumetricFogBlurPS.hlsl (フォグ用バイラテラルブラー)
// ==========================================
#include "FullScreenQuad.hlsli"

Texture2D FogMap : register(t0);
Texture2D GBuffer2 : register(t1); // WorldPosDepth

SamplerState pointSampler : register(s2);

float4 main(VS_OUT pin) : SV_TARGET
{
    uint width, height;
    FogMap.GetDimensions(width, height);
    float2 texelSize = 1.0f / float2(width, height);

    float3 centerPos = GBuffer2.Sample(pointSampler, pin.texcoord).xyz;
    float centerDepth = GBuffer2.Sample(pointSampler, pin.texcoord).w;

    float3 totalFog = float3(0, 0, 0);
    float totalWeight = 0.0f;

    // ぼかし半径 (フォグは低周波なので少し広めにとる)
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

            // 空間的ウェイト (ガウス分布)
            float spatialWeight = exp(-(x * x + y * y) / 8.0f);

            // 深度ウェイト (エッジをまたがないようにする)
            // 背景(depth >= 1.0)同士なら混ぜる、前景と背景は混ぜない
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
                depthWeight = 0.0f; // 前景と背景は完全に分離
            }

            float weight = spatialWeight * depthWeight;

            totalFog += sampleFog * weight;
            totalWeight += weight;
        }
    }

    return float4(totalFog / max(totalWeight, 0.0001f), 1.0f);
}