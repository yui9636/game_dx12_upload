#include "FullScreenQuad.hlsli"

Texture2D ssgiMap : register(t0); // 先ほど作った生SSGI
Texture2D normalMap : register(t1); // Target 1
Texture2D worldPosMap : register(t2); // Target 2

SamplerState pointSampler : register(s2);
SamplerState linearSampler : register(s3);


float4 main(VS_OUT pin) : SV_TARGET
{
    uint width, height;
    ssgiMap.GetDimensions(width, height);
    float2 texelSize = 1.0f / float2(width, height);

    // 中心ピクセルの情報
    float3 centerNormal = normalize(normalMap.Sample(pointSampler, pin.texcoord).xyz);
    float3 centerPos = worldPosMap.Sample(pointSampler, pin.texcoord).xyz;

    float3 totalColor = float3(0, 0, 0);
    float totalWeight = 0.0f;

    // 5x5 ピクセル周辺をサンプリング
    const int BLUR_RADIUS = 2;

    [unroll]
    for (int y = -BLUR_RADIUS; y <= BLUR_RADIUS; ++y)
    {
        [unroll]
        for (int x = -BLUR_RADIUS; x <= BLUR_RADIUS; ++x)
        {
            float2 offset = float2(x, y) * texelSize;
            float2 sampleUV = pin.texcoord + offset;

            float3 sampleColor = ssgiMap.SampleLevel(linearSampler, sampleUV, 0).rgb;
            float3 sampleNormal = normalize(normalMap.SampleLevel(pointSampler, sampleUV, 0).xyz);
            float3 samplePos = worldPosMap.SampleLevel(pointSampler, sampleUV, 0).xyz;

            // 【バイラテラル・ウェイト計算】
            // 1. 空間的距離（遠いピクセルほど影響を減らす）
            float spatialWeight = exp(-(x * x + y * y) / 8.0f);
            
            // 2. 深度/位置の違い（段差がある場合は重みを0にして、光が漏れるのを防ぐ）
            float posDiff = length(centerPos - samplePos);
            float depthWeight = exp(-posDiff * 10.0f);

            // 3. 法線の違い（向いている方向が違う面同士は混ぜない）
            float normalWeight = pow(saturate(dot(centerNormal, sampleNormal)), 16.0f);

            // 総合ウェイト
            float weight = spatialWeight * depthWeight * normalWeight;

            totalColor += sampleColor * weight;
            totalWeight += weight;
        }
    }

    // ブラーが完了した滑らかな間接光を出力
    return float4(totalColor / max(totalWeight, 0.0001f), 1.0f);
}