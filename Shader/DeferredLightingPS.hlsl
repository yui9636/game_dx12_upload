// DeferredLightingPS.hlsl（SSGI 統合、物理影修正、プローブ反射対応）
#include "PBR.hlsli"
#include "ShadingFunctions.hlsli"

Texture2D GBuffer0 : register(t0);
Texture2D GBuffer1 : register(t1);
Texture2D GBuffer2 : register(t2);
Texture2D AOMap : register(t3);
Texture2DArray shadowMap : register(t4);

// 空間ブラー済みの SSGI バッファなど
Texture2D SSGIMap : register(t5);
Texture2D FogMap : register(t6);
Texture2D SSRMap : register(t7);

// ローカル反射プローブ（エンジン内ベイカーで撮影したキューブマップ）
TextureCube ProbeMap : register(t8);

SamplerComparisonState shadowSampler : register(s1);
TextureCube diffuse_iem : register(t33);
TextureCube specular_pmrem : register(t34);
Texture2D lut_ggx : register(t35);

SamplerState PointSamp : register(s2);
SamplerState LinearSamp : register(s3);

static const float PI_INV = 1.0f / 3.14159265f;

// 影の計算。安定した viewDepth を受け取り、ジッターの影響を避ける。
float CalcShadowFactorCSM_Stable(float3 worldPos, float stableViewDepth)
{
    uint cascadeIndex = 0;
    if (stableViewDepth > cascadeSplits.x)
        cascadeIndex = 1;
    if (stableViewDepth > cascadeSplits.y)
        cascadeIndex = 2;
    if (stableViewDepth > cascadeSplits.z)
        return 1.0f;
    
    float4 lightPos = mul(float4(worldPos, 1.0f), lightViewProjections[cascadeIndex]);
    float3 projCoords = lightPos.xyz / lightPos.w;
    projCoords.x = projCoords.x * 0.5f + 0.5f;
    projCoords.y = -projCoords.y * 0.5f + 0.5f;
    
    if (projCoords.x < 0.0f || projCoords.x > 1.0f || projCoords.y < 0.0f || projCoords.y > 1.0f || projCoords.z > 1.0f)
        return 1.0f;
        
    float currentDepth = projCoords.z - shadowBias_CSM.x;
    float shadow = 0.0f;
    const float2 texelSize = float2(1.0f / 4096.0f, 1.0f / 4096.0f);
    
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

struct VS_OUT_QUAD
{
    float4 pos : SV_POSITION;
    float2 uv : TEXCOORD;
};

float4 main(VS_OUT_QUAD pin) : SV_TARGET
{
    // GBuffer から法線、マテリアル、ワールド位置を復元し、背景ピクセルは破棄する。
    float4 g1 = GBuffer1.Sample(PointSamp, pin.uv);
    if (length(g1.xyz) < 0.1f)
        discard; // 背景なら描画しない。

    float4 g0 = GBuffer0.Sample(PointSamp, pin.uv);
    float4 g2 = GBuffer2.Sample(PointSamp, pin.uv);

    float3 albedoLin = g0.rgb;
    float metallic = g0.a;
    float3 N = normalize(g1.xyz);
    float roughness = g1.a;
    float3 worldPos = g2.xyz;

    float4 stableClipPos = mul(float4(worldPos, 1.0f), viewProjectionUnjittered);
    float stableViewDepth = stableClipPos.w;

    float3 V = normalize(cameraPosition.xyz - worldPos);
    float3 F0 = lerp(float3(0.04f, 0.04f, 0.04f), albedoLin, metallic);
    float3 Lo = float3(0.0f, 0.0f, 0.0f);
    // 物理ベースの扱いとして、影は太陽光（平行光源）にだけ掛ける。
    float shadowFactor = CalcShadowFactorCSM_Stable(worldPos, stableViewDepth);
    float3 dirShadow = lerp(shadowColor_CSM.rgb, float3(1.0f, 1.0f, 1.0f), shadowFactor);
    // 平行光を物理ベース BRDF で評価し、カスケード影を反映する。
    // 平行光源
    {
        float3 L = normalize(-lightDirection.xyz);
        float3 H = normalize(V + L);
        float NdotL = max(dot(N, L), 0.0f);
        float NdotV = max(dot(N, V), 1e-4f);
        float NdotH = max(dot(N, H), 0.0f);
        float VdotH = max(dot(V, H), 0.0f);
        
        if (NdotL > 0.0f)
        {
            float3 F = CalcFresnel(F0, VdotH);
            float D = CalcNormalDistributionFunction(NdotH, roughness);
            float G = CalcGeometryFunction(NdotL, NdotV, roughness);
            float3 Spec = (D * G * F) / max(4.0f * NdotL * NdotV, 1e-4f);
            float3 Diff = (1.0f - F) * (1.0f - metallic) * albedoLin * PI_INV;
            
            // 太陽光にだけ影を掛ける。
            Lo += (Diff + Spec) * lightColor.rgb * NdotL * dirShadow;
        }
    }

    // 点光源
    for (int i = 0; i < (int) pointLightCount; ++i)
    {
        PointLight light = pointLights[i];
        float3 L_vec = light.position - worldPos;
        float dist = length(L_vec);
        if (dist >= light.range)
            continue;

        float3 L = normalize(L_vec);
        float3 H = normalize(V + L);
        float NdotL = max(dot(N, L), 0.0f);
        float NdotV = max(dot(N, V), 1e-4f);
        float NdotH = max(dot(N, H), 0.0f);
        float VdotH = max(dot(V, H), 0.0f);
        
        if (NdotL > 0.0f)
        {
            float attenuation = saturate(1.0f - (dist / light.range));
            attenuation *= attenuation;
            float3 F = CalcFresnel(F0, VdotH);
            float D = CalcNormalDistributionFunction(NdotH, roughness);
            float G = CalcGeometryFunction(NdotL, NdotV, roughness);
            float3 Spec = (D * G * F) / max(4.0f * NdotL * NdotV, 1e-4f);
            float3 Diff = (1.0f - F) * (1.0f - metallic) * albedoLin * PI_INV;
            
            Lo += (Diff + Spec) * (light.color * light.intensity * attenuation) * NdotL;
        }
    }
    // 環境光、反射プローブ、SSR、AO、SSGI を合成して間接光を作る。
    float3 diffIBL = DiffuseIBL(N, -V, roughness, albedoLin * (1.0f - metallic), F0, diffuse_iem, LinearSamp);
    
    // ベースとなる空の反射
    float3 specIBL = SpecularIBL(N, -V, roughness, F0, lut_ggx, specular_pmrem, LinearSamp);
    // リフレクションプローブを合成し、空の反射を上書きする。
    float3 R = reflect(-V, N);
    float3 probeColor = ProbeMap.SampleLevel(LinearSamp, R, 0).rgb;
    
    // プローブが黒でなければ、空の代わりにプローブを使う。
    float probeWeight = saturate(dot(probeColor, float3(0.33f, 0.33f, 0.33f)) * 100.0f);
    
    // プローブ色にもフレネルとラフネスの減衰をかける。
    float NdotV_probe = max(dot(N, V), 0.0f);
    float2 envBRDF = lut_ggx.SampleLevel(LinearSamp, float2(NdotV_probe, roughness), 0).rg;
    float3 probeSpec = probeColor * (F0 * envBRDF.x + envBRDF.y);
    
    specIBL = lerp(specIBL, probeSpec, probeWeight);
    // スクリーンスペース反射を合成し、プローブをさらに上書きする。
    float3 ssrColor = SSRMap.SampleLevel(LinearSamp, pin.uv, 0).rgb;
    
    // 反射色が有効ならスクリーンスペース反射を優先し、なければプローブまたは空の反射を使う。
    float ssrWeight = saturate(dot(ssrColor, float3(0.33f, 0.33f, 0.33f)) * 10.0f);
    specIBL = lerp(specIBL, ssrColor, ssrWeight);
    
    
    // AO のバイラテラルブラー
    float ao = 0.0f;
    float weightSum = 0.0f;
    float2 texelSize = float2(1.0f / renderW, 1.0f / renderH);
    float blurWeights5[5] = { 1.0f, 4.0f, 6.0f, 4.0f, 1.0f };
    
    [unroll]
    for (int y = -2; y <= 2; ++y)
    {
        [unroll]
        for (int x = -2; x <= 2; ++x)
        {
            float2 offsetUV = pin.uv + float2(x, y) * texelSize;
            float sampleAO = AOMap.SampleLevel(PointSamp, offsetUV, 0).r;
            float3 samplePos = GBuffer2.SampleLevel(PointSamp, offsetUV, 0).xyz;
            float distDiff = length(samplePos - worldPos);
            float w = blurWeights5[y + 2] * blurWeights5[x + 2];
            float weight = (distDiff < 0.6f) ? w : 0.0f;
            ao += sampleAO * weight;
            weightSum += weight;
        }
    }
    ao = (weightSum > 0.0001f) ? (ao / weightSum) : 1.0f;
    
    diffIBL *= ao;
    specIBL *= lerp(1.0f, ao, 0.8f); // 鏡面反射に対する AO は少し弱める。
    // SSGI を合成し、間接ディフューズ光として加算する。
    // ハーフ解像度の SSGI を滑らかに拡大するため、線形サンプラーを使う。
    float3 ssgiColor = SSGIMap.SampleLevel(LinearSamp, pin.uv, 0).rgb;
    
    // SSGI はバウンスした間接光。表面色を乗算し、AO で隅の光漏れを抑える。
    float3 indirectDiffuse = ssgiColor * albedoLin * (1.0f - metallic) * ao;
    // 直接光、間接光、反射、フォグを合算して最終色を返す。
    // 物理則に基づき、直接光、環境光、SSGI、スペキュラ反射を足し合わせる。
    float3 color = Lo + diffIBL + indirectDiffuse + specIBL;

    
    float3 fogColor = FogMap.SampleLevel(LinearSamp, pin.uv, 0).rgb;
    color += fogColor;
    
    
    return float4(color, 1.0f);
}

