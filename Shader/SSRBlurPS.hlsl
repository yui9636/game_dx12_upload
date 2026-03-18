// ==========================================
// SSRBlurPS.hlsl (材質に応じた反射のぼかし)
// ==========================================
#include "FullScreenQuad.hlsli"

Texture2D SSRMap : register(t0); // 生のSSR
Texture2D GBuffer1 : register(t1); // Normal + Roughness
Texture2D GBuffer2 : register(t2); // WorldPos

SamplerState pointSampler : register(s2);
SamplerState linearSampler : register(s3);


float4 main(VS_OUT pin) : SV_TARGET
{
    uint width, height;
    SSRMap.GetDimensions(width, height);
    float2 texelSize = 1.0f / float2(width, height);

    float roughness = GBuffer1.Sample(pointSampler, pin.texcoord).a;
    float3 centerNormal = normalize(GBuffer1.Sample(pointSampler, pin.texcoord).xyz);
    float3 centerPos = GBuffer2.Sample(pointSampler, pin.texcoord).xyz;

    // ★ 鏡面（ツルツル）ならブラーをかけずにそのまま返す (負荷削減 & くっきり反射)
    if (roughness < 0.05f)
    {
        return SSRMap.SampleLevel(pointSampler, pin.texcoord, 0);
    }

    float3 totalColor = float3(0, 0, 0);
    float totalWeight = 0.0f;

    // ★ ラフネスが高いほどブラーの範囲を広げる
    int blurRadius = (int) lerp(1.0f, 4.0f, roughness);

    [unroll]
    for (int y = -3; y <= 3; ++y)
    {
        // blurRadius 以上のループは実質無視する
        if (abs(y) > blurRadius)
            continue;

        [unroll]
        for (int x = -3; x <= 3; ++x)
        {
            if (abs(x) > blurRadius)
                continue;

            float2 offset = float2(x, y) * texelSize * 1.5f; // 1.5fは広がり係数
            float2 sampleUV = pin.texcoord + offset;

            float3 sampleColor = SSRMap.SampleLevel(linearSampler, sampleUV, 0).rgb;
            float3 sampleNormal = normalize(GBuffer1.SampleLevel(pointSampler, sampleUV, 0).xyz);
            float3 samplePos = GBuffer2.SampleLevel(pointSampler, sampleUV, 0).xyz;

            float spatialWeight = exp(-(x * x + y * y) / (2.0f * blurRadius * blurRadius));
            float normalWeight = pow(saturate(dot(centerNormal, sampleNormal)), 8.0f);
            float depthWeight = exp(-length(centerPos - samplePos) * 2.0f);

            float weight = spatialWeight * normalWeight * depthWeight;

            totalColor += sampleColor * weight;
            totalWeight += weight;
        }
    }

    return float4(totalColor / max(totalWeight, 0.0001f), 1.0f);
}