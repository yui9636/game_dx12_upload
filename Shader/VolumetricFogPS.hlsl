// ==========================================
// VolumetricFogPS.hlsl (空間の光の散乱)
// ==========================================
#include "FullScreenQuad.hlsli"

Texture2D GBuffer2 : register(t0); // Target 2: WorldPosDepth
Texture2DArray shadowMap : register(t4); // CSM ShadowMap

SamplerState pointSampler : register(s2);
SamplerComparisonState shadowSampler : register(s1);

struct PointLight
{
    float3 position;
    float range;
    float3 color;
    float intensity;
};

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
    PointLight pointLights[8];
};

cbuffer CbShadowMap : register(b4)
{
    matrix lightViewProjections[3];
    float4 cascadeSplits;
    float4 shadowColor_CSM;
    float4 shadowBias_CSM;
};

// --- ヘルパー関数 ---

// Henyey-Greenstein位相関数 (光の散乱の偏りを計算)
float ComputeHenyeyGreenstein(float g, float cosTheta)
{
    float g2 = g * g;
    float num = 1.0f - g2;
    float denom = 1.0f + g2 - 2.0f * g * cosTheta;
    return num / (4.0f * 3.14159265f * pow(denom, 1.5f));
}

// 空間の任意の点の影を判定
float GetShadowFactor(float3 worldPos, float viewDepth)
{
    uint cascadeIndex = 0;
    if (viewDepth > cascadeSplits.x)
        cascadeIndex = 1;
    if (viewDepth > cascadeSplits.y)
        cascadeIndex = 2;
    if (viewDepth > cascadeSplits.z)
        return 1.0f; // 範囲外は影なし(1.0)
    
    float4 lightPos = mul(float4(worldPos, 1.0f), lightViewProjections[cascadeIndex]);
    float3 projCoords = lightPos.xyz / lightPos.w;
    projCoords.x = projCoords.x * 0.5f + 0.5f;
    projCoords.y = -projCoords.y * 0.5f + 0.5f;
    
    if (projCoords.x < 0.0f || projCoords.x > 1.0f || projCoords.y < 0.0f || projCoords.y > 1.0f || projCoords.z > 1.0f)
        return 1.0f;
        
    float currentDepth = projCoords.z - shadowBias_CSM.x;
    float shadow = 0.0f;
    const float2 texelSize = float2(1.0f / 4096.0f, 1.0f / 4096.0f);
    
    // フォグ用の影は少し軽めに3x3 PCF
    [unroll]
    for (int x = -1; x <= 1; ++x)
    {
        [unroll]
        for (int y = -1; y <= 1; ++y)
        {
            float3 uvw = float3(projCoords.xy + float2(x, y) * texelSize, (float) cascadeIndex);
            shadow += shadowMap.SampleCmpLevelZero(shadowSampler, uvw, currentDepth);
        }
    }
    return shadow / 9.0f;
}

// 疑似乱数 (Dithering用)
float rand(float2 uv)
{
    return frac(sin(dot(uv, float2(12.9898, 78.233))) * 43758.5453);
}

// --- メイン関数 ---
float4 main(VS_OUT pin) : SV_TARGET
{
    float4 wpd = GBuffer2.Sample(pointSampler, pin.texcoord);
    float3 targetWorldPos = wpd.xyz;
    
    // 背景(空)の場合は遠くまでレイを飛ばす
    if (wpd.w >= 1.0f)
    {
        // カメラの視線ベクトルを取得して遠方(100m先など)をターゲットにする
        float4 clipPos = float4(pin.texcoord.x * 2.0f - 1.0f, -(pin.texcoord.y * 2.0f - 1.0f), 1.0f, 1.0f);
        float4 worldPos_unproj = mul(clipPos, prevViewProjection); // 逆行列の代わりにprev等で代用できるか考える必要がありますが、今回は簡単な近似を使用します
        // 簡易的にカメラ方向ベクトルを作成
    }

    float3 startPos = cameraPosition.xyz;
    float3 rayVec = targetWorldPos - startPos;
    
    // 背景の場合はレイの長さを制限する (例: 最大50m)
    float maxDistance = 50.0f;
    float distToTarget = length(rayVec);
    float rayLength = min(distToTarget, maxDistance);
    
    float3 rayDir = rayVec / distToTarget;

    // パラメータ
    const int STEP_COUNT = 16;
    float stepSize = rayLength / (float) STEP_COUNT;
    
    // ディザリング (縞模様バンディング対策)
    // 開始位置をピクセルごとに少しずらす
    float dither = rand(pin.texcoord + jitterX) * stepSize;
    float3 currentPos = startPos + rayDir * dither;

    float3 accumulatedFog = float3(0, 0, 0);

    // 太陽の方向ベクトル
    float3 L_dir = normalize(-lightDirection.xyz);
    
    // 位相関数用の角度 (カメラ視線と光の角度)
    float cosTheta = dot(rayDir, L_dir);
    // gパラメータ: 0.0=等方散乱, 0.7~0.9=強い前方散乱 (太陽の方を見るとまぶしい)
    float phaseDirectional = ComputeHenyeyGreenstein(0.7f, cosTheta);

    // フォグの濃さ・散乱係数
    float scatteringCoeff = 0.05f;

    for (int i = 0; i < STEP_COUNT; ++i)
    {
        // 現在地のViewDepthを近似計算 (本来は行列を掛けるべきですが、距離で代用)
        float currentViewDepth = length(currentPos - startPos);

        // 1. 太陽光の散乱
        float shadowFactor = GetShadowFactor(currentPos, currentViewDepth);
        float3 directionalLightInscattering = lightColor.rgb * shadowFactor * phaseDirectional;

        // 2. ポイントライトの散乱 (影なし・超軽量)
        float3 pointLightInscattering = float3(0, 0, 0);
        for (int p = 0; p < (int) pointLightCount; ++p)
        {
            float3 L_vec = pointLights[p].position - currentPos;
            float dist = length(L_vec);
            if (dist < pointLights[p].range)
            {
                float attenuation = saturate(1.0f - (dist / pointLights[p].range));
                attenuation *= attenuation; // 距離減衰
                
                // ポイントライト用の位相関数 (全方位に広がるようにgを小さく)
                float3 pL_dir = normalize(L_vec);
                float pCosTheta = dot(rayDir, pL_dir);
                float pPhase = ComputeHenyeyGreenstein(0.2f, pCosTheta);
                
                pointLightInscattering += pointLights[p].color * pointLights[p].intensity * attenuation * pPhase;
            }
        }

        // 蓄積 (距離に応じて積分)
        accumulatedFog += (directionalLightInscattering + pointLightInscattering) * scatteringCoeff * stepSize;

        // 次のステップへ
        currentPos += rayDir * stepSize;
    }

    return float4(accumulatedFog, 1.0f);
}