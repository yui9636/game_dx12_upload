// ==========================================
// SSGIPS.hlsl (Screen Space Global Illumination - 完全版)
// ==========================================
#include "FullScreenQuad.hlsli"

// GlobalRootSignature がバインドしてくれる定数バッファ (b7)
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

// 入力テクスチャ
Texture2D normalMap : register(t0); // Target 1: Normal
Texture2D worldPosMap : register(t1); // Target 2: WorldPosDepth
Texture2D prevSceneMap : register(t2); // PrevScene: 過去の光の記憶

SamplerState pointSampler : register(s2);
SamplerState linearSampler : register(s3);


// 疑似乱数ジェネレーター (ホワイトノイズ)
float rand(float2 uv, float seed)
{
    return frac(sin(dot(uv.xy, float2(12.9898, 78.233)) + seed) * 43758.5453);
}

// ★修正1：プロ仕様のコサイン重み付き半球サンプリング (光が自然に広がり、丸いノイズが消える)
float3 GetCosineWeightedHemisphereDir(float3 normal, float2 uv, float seed)
{
    float u = rand(uv, seed);
    float v = rand(uv, seed + 1.234f);
    
    float theta = 2.0f * 3.14159265f * u;
    float r = sqrt(v);
    float x = r * cos(theta);
    float y = r * sin(theta);
    float z = sqrt(max(0.0f, 1.0f - v));
    
    // TBNマトリクスを作成して接空間からワールド空間へ変換
    float3 up = abs(normal.z) < 0.999f ? float3(0, 0, 1) : float3(1, 0, 0);
    float3 tangent = normalize(cross(up, normal));
    float3 bitangent = cross(normal, tangent);
    
    return normalize(tangent * x + bitangent * y + normal * z);
}

float4 main(VS_OUT pin) : SV_TARGET
{
    float4 worldPosDepth = worldPosMap.Sample(pointSampler, pin.texcoord);
    if (worldPosDepth.w >= 1.0f)
        return float4(0, 0, 0, 0); // 背景はスキップ

    float3 worldPos = worldPosDepth.xyz;
    float3 normal = normalize(normalMap.Sample(pointSampler, pin.texcoord).xyz);

    // ★ パラメータ調整：精度を上げつつ、レイが届く距離を調整
    const int RAY_COUNT = 6; // レイの数（少し増やす）
    const int STEP_COUNT = 10; // 探索精度（少し増やす）
    const float RAY_LENGTH = 2.0f; // 光が届く距離 (m)
    const float THICKNESS = 0.5f; // 裏抜け防止

    float3 indirectLight = float3(0, 0, 0);
    float seed = (jitterX * 10.0f) + pin.texcoord.x + (jitterY * 10.0f) + pin.texcoord.y;

    for (int i = 0; i < RAY_COUNT; i++)
    {
        float3 rayDir = GetCosineWeightedHemisphereDir(normal, pin.texcoord, seed + float(i));
        float stepSize = RAY_LENGTH / (float) STEP_COUNT;
        
        // レイの開始位置を法線方向に少し浮かせる（自己交差ノイズ防止）
        float3 rayPos = worldPos + normal * 0.05f;

        for (int j = 0; j < STEP_COUNT; j++)
        {
            rayPos += rayDir * stepSize;
            
            // ★修正2：ジッターの無い行列を使って、現在画面の「本当の位置」を探す
            float4 clipPos = mul(float4(rayPos, 1.0f), viewProjectionUnjittered);
            clipPos /= clipPos.w;
            float2 sampleUV = clipPos.xy * float2(0.5f, -0.5f) + 0.5f;

            if (any(sampleUV < 0.0f) || any(sampleUV > 1.0f))
                break;

            float4 sampleWPD = worldPosMap.SampleLevel(pointSampler, sampleUV, 0);
            
            // ★修正3：レイが「背景」に当たった場合は絶対に無視する！（丸い光の原因を抹殺）
            if (sampleWPD.w >= 1.0f)
                continue;

            float3 sampleWorldPos = sampleWPD.xyz;

            float rayDist = length(rayPos - cameraPosition.xyz);
            float sampleDist = length(sampleWorldPos - cameraPosition.xyz);

            // 衝突判定
            if (rayDist > sampleDist && (rayDist - sampleDist) < THICKNESS)
            {
                
                // =========================================================
                // ★修正4：時間軸UVズレの完全解消（Reprojection）
                // 衝突したワールド座標が「前のフレームで画面のどこにあったか」を逆算する！
                // =========================================================
                float4 prevClipPos = mul(float4(sampleWorldPos, 1.0f), prevViewProjection);
                prevClipPos /= prevClipPos.w;
                float2 prevUV = prevClipPos.xy * float2(0.5f, -0.5f) + 0.5f;

                // 前のフレームで画面外だった場合は諦める
                if (any(prevUV < 0.0f) || any(prevUV > 1.0f))
                    break;

                // 過去の光をサンプリング
                float3 hitColor = prevSceneMap.SampleLevel(linearSampler, prevUV, 0).rgb;
                
                // 距離による減衰
                float attenuation = saturate(1.0f - (float(j) / float(STEP_COUNT)));
                
                // ★コサイン重み付きサンプリングのおかげで、ここで NdotL を掛ける必要がなくなりました
                // 強度係数（2.0f）は、お好みで調整してください
                indirectLight += hitColor * attenuation * 2.0f;
                break;
            }
        }
    }

    indirectLight /= float(RAY_COUNT);
    return float4(indirectLight, 1.0f);
}
