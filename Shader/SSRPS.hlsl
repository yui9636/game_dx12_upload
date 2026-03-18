#include "FullScreenQuad.hlsli"

Texture2D GBuffer0 : register(t0);
Texture2D GBuffer1 : register(t1);
Texture2D GBuffer2 : register(t2);
Texture2D PrevSceneMap : register(t3);

SamplerState pointSampler : register(s2); // ★唯百様の正解環境(s0)
SamplerState linearSampler : register(s3); // ★唯百様の正解環境(s2)

cbuffer CbScene : register(b7)
{
    matrix viewProjection;
    matrix viewProjectionUnjittered;
    matrix prevViewProjection;
    float4 lightDirection;
    float4 lightColor;
    float4 cameraPosition;
    matrix lightViewProjection;
    float4 shadowColor;
    float shadowTexelSize;
    float jitterX;
    float jitterY;
    float renderW;
    float renderH;
    float pointLightCount;
    float prevJitterX;
    float prevJitterY;
};

float4 main(VS_OUT pin) : SV_TARGET
{
    // 【究極のデバッグ】もし画面が真っ黒なら、原因2の「PrevSceneのコピー忘れ」が確定します！
    // return PrevSceneMap.SampleLevel(linearSampler, pin.texcoord, 0);

    float4 wpd = GBuffer2.Sample(pointSampler, pin.texcoord);
    if (wpd.w >= 1.0f)
        return float4(0, 0, 0, 0);

    float4 g0 = GBuffer0.Sample(pointSampler, pin.texcoord);
    float4 g1 = GBuffer1.Sample(pointSampler, pin.texcoord);

    float metallic = g0.a;
    float roughness = g1.a;
    float3 normal = normalize(g1.xyz);
    float3 worldPos = wpd.xyz;

    if (roughness > 0.8f)
        return float4(0, 0, 0, 0);

    float3 viewDir = normalize(worldPos - cameraPosition.xyz);
    float3 reflectDir = normalize(reflect(viewDir, normal));

    // =========================================================
    // ★ 究極設定：GPUの力技で「絶対に反射を見つけ出す」
    // =========================================================
    const int MAX_STEPS = 1000; // 限界突破の1000回ループ！
    const float STEP_SIZE = 0.1f; // 10cmの超精密な歩幅（100m先まで絶対にすり抜けず探す）
    const float THICKNESS = 0.5f; // 厚み判定も安全マージンを確保
    

    float3 rayPos = worldPos + normal * 0.05f;
    float3 hitColor = float3(0, 0, 0);

    
    float fade = 0.0f;

    for (int i = 0; i < MAX_STEPS; i++)
    {
        rayPos += reflectDir * STEP_SIZE;

        float4 clipPos = mul(float4(rayPos, 1.0f), viewProjectionUnjittered);
        clipPos /= clipPos.w;
        float2 sampleUV = clipPos.xy * float2(0.5f, -0.5f) + 0.5f;

        if (any(sampleUV < 0.0f) || any(sampleUV > 1.0f))
            break;

        float4 sampleWPD = GBuffer2.SampleLevel(pointSampler, sampleUV, 0);
        if (sampleWPD.w >= 1.0f)
            continue;

        float3 sampleWorldPos = sampleWPD.xyz;

        float rayDist = length(rayPos - cameraPosition.xyz);
        float sampleDist = length(sampleWorldPos - cameraPosition.xyz);

        if (rayDist > sampleDist && (rayDist - sampleDist) < THICKNESS)
        {
            
            // ★ 修正: prev行列が壊れているリスクを排除し、現在の揺れていない行列を使用
            float4 safeClipPos = mul(float4(sampleWorldPos, 1.0f), viewProjectionUnjittered);
            safeClipPos /= safeClipPos.w;
            float2 safeUV = safeClipPos.xy * float2(0.5f, -0.5f) + 0.5f;

            if (any(safeUV < 0.0f) || any(safeUV > 1.0f))
                break;

            hitColor = PrevSceneMap.SampleLevel(linearSampler, safeUV, 0).rgb;

            float2 edgeFade = smoothstep(0.0f, 0.1f, sampleUV) * smoothstep(1.0f, 0.9f, sampleUV);
            fade = edgeFade.x * edgeFade.y;
            break;
        }
    }

    float3 f0 = lerp(float3(0.04f, 0.04f, 0.04f), g0.rgb, metallic);
    float NdotV = max(dot(normal, -viewDir), 0.0f);
    float3 fresnel = f0 + (1.0f - f0) * pow(1.0f - NdotV, 5.0f);
    float roughnessFade = 1.0f - smoothstep(0.3f, 0.8f, roughness);

    return float4(hitColor * fresnel * fade * roughnessFade, 1.0f);
}
