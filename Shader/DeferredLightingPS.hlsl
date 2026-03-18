////// ==========================================
////// DeferredLightingPS.hlsl (完全修正・振動抹殺版)
////// ==========================================
//#include "PBR.hlsli"
//#include "ShadingFunctions.hlsli"

//Texture2D GBuffer0 : register(t0);
//Texture2D GBuffer1 : register(t1);
//Texture2D GBuffer2 : register(t2);
//Texture2D AOMap : register(t3);

//Texture2DArray shadowMap : register(t4);
//SamplerComparisonState shadowSampler : register(s1);
//TextureCube diffuse_iem : register(t33);
//TextureCube specular_pmrem : register(t34);
//Texture2D lut_ggx : register(t35);

//SamplerState PointSamp : register(s2);
//SamplerState LinearSamp : register(s3);

//static const float PI_INV = 1.0f / 3.14159265f;

//// 影の計算（安定した viewDepth を受け取るように変更）
//float CalcShadowFactorCSM_Stable(float3 worldPos, float stableViewDepth)
//{
//    uint cascadeIndex = 0;
//    if (stableViewDepth > cascadeSplits.x)
//        cascadeIndex = 1;
//    if (stableViewDepth > cascadeSplits.y)
//        cascadeIndex = 2;
//    if (stableViewDepth > cascadeSplits.z)
//        return 1.0f;
    
//    float4 lightPos = mul(float4(worldPos, 1.0f), lightViewProjections[cascadeIndex]);
//    float3 projCoords = lightPos.xyz / lightPos.w;
//    projCoords.x = projCoords.x * 0.5f + 0.5f;
//    projCoords.y = -projCoords.y * 0.5f + 0.5f;
    
//    if (projCoords.x < 0.0f || projCoords.x > 1.0f || projCoords.y < 0.0f || projCoords.y > 1.0f || projCoords.z > 1.0f)
//        return 1.0f;
        
//    float currentDepth = projCoords.z - shadowBias_CSM.x;
//    float shadow = 0.0f;
//    const float2 texelSize = float2(1.0f / 4096.0f, 1.0f / 4096.0f);
    
//    [unroll]
//    for (int x = -1; x <= 1; ++x)
//    {
//        [unroll]
//        for (int y = -1; y <= 1; ++y)
//        {
//            float3 uvw = float3(projCoords.xy + float2(x, y) * texelSize, (float) cascadeIndex);
//            shadow += shadowMap.SampleCmpLevelZero(shadowSampler, uvw, currentDepth);
//        }
//    }
//    return shadow / 9.0f;
//}

//struct VS_OUT_QUAD
//{
//    float4 pos : SV_POSITION;
//    float2 uv : TEXCOORD;
//};

//float4 main(VS_OUT_QUAD pin) : SV_TARGET
//{
//    // --- [ 1. G-Buffer サンプリング ] ---
//    float4 g1 = GBuffer1.Sample(PointSamp, pin.uv);
//    if (length(g1.xyz) < 0.1f)
//    {
//        discard;
//    } // 背景なら描画しない

//    float4 g0 = GBuffer0.Sample(PointSamp, pin.uv);
//    float4 g2 = GBuffer2.Sample(PointSamp, pin.uv);

//    float3 albedoLin = g0.rgb;
//    float metallic = g0.a;
//    float3 N = normalize(g1.xyz);
//    float roughness = g1.a;
//    float3 worldPos = g2.xyz;

//    // =========================================================
//    // ★ 振動対策の核心：揺れていない行列で「真の距離」を測る
//    // =========================================================
//    // viewProjection (揺れている) ではなく viewProjectionUnjittered を使用
//    float4 stableClipPos = mul(float4(worldPos, 1.0f), viewProjectionUnjittered);
//    float stableViewDepth = stableClipPos.w;

//    // --- [ 2. ライティング計算 ] ---
//    float3 V = normalize(cameraPosition.xyz - worldPos);
//    float3 F0 = lerp(float3(0.04, 0.04, 0.04), albedoLin, metallic);
//    float3 Lo = float3(0, 0, 0);

//    // 平行光源
//    {
//        float3 L = normalize(-lightDirection.xyz);
//        float3 H = normalize(V + L);
//        float NdotL = max(dot(N, L), 0.0);
//        float NdotV = max(dot(N, V), 1e-4);
//        float NdotH = max(dot(N, H), 0.0);
//        float VdotH = max(dot(V, H), 0.0);

//        if (NdotL > 0.0)
//        {
//            float3 F = CalcFresnel(F0, VdotH);
//            float D = CalcNormalDistributionFunction(NdotH, roughness);
//            float G = CalcGeometryFunction(NdotL, NdotV, roughness);
//            float3 Spec = (D * G * F) / max(4.0f * NdotL * NdotV, 1e-4);
//            float3 Diff = (1.0f - F) * (1.0f - metallic) * albedoLin * PI_INV;
//            Lo += (Diff + Spec) * lightColor.rgb * NdotL;
//        }
//    }

//    // 点光源
//    for (int i = 0; i < (int) pointLightCount; ++i)
//    {
//        PointLight light = pointLights[i];
//        float3 L_vec = light.position - worldPos;
//        float dist = length(L_vec);
//        if (dist >= light.range)
//            continue;

//        float3 L = normalize(L_vec);
//        float3 H = normalize(V + L);
//        float NdotL = max(dot(N, L), 0.0);
//        float NdotV = max(dot(N, V), 1e-4);
//        float NdotH = max(dot(N, H), 0.0);
//        float VdotH = max(dot(V, H), 0.0);

//        if (NdotL > 0.0)
//        {
//            float attenuation = saturate(1.0 - (dist / light.range));
//            attenuation *= attenuation;
//            float3 F = CalcFresnel(F0, VdotH);
//            float D = CalcNormalDistributionFunction(NdotH, roughness);
//            float G = CalcGeometryFunction(NdotL, NdotV, roughness);
//            float3 Spec = (D * G * F) / max(4.0f * NdotL * NdotV, 1e-4);
//            float3 Diff = (1.0f - F) * (1.0f - metallic) * albedoLin * PI_INV;
//            Lo += (Diff + Spec) * (light.color * light.intensity * attenuation) * NdotL;
//        }
//    }

//    // IBL
//    float3 diffIBL = DiffuseIBL(N, -V, roughness, albedoLin * (1.0f - metallic), F0, diffuse_iem, LinearSamp);
//    float3 specIBL = SpecularIBL(N, -V, roughness, F0, lut_ggx, specular_pmrem, LinearSamp);

//    // --- [ 3. AO バイラテラルブラー (安定化版) ] ---
//    float ao = 0.0f;
//    float weightSum = 0.0f;
//    float2 texelSize = float2(1.0f / renderW, 1.0f / renderH);
//    float blurWeights5[5] = { 1.0f, 4.0f, 6.0f, 4.0f, 1.0f };

//    [unroll]
//    for (int y = -2; y <= 2; ++y)
//    {
//        [unroll]
//        for (int x = -2; x <= 2; ++x)
//        {
//            float2 offsetUV = pin.uv + float2(x, y) * texelSize;
//            float sampleAO = AOMap.SampleLevel(PointSamp, offsetUV, 0).r;
//            float3 samplePos = GBuffer2.SampleLevel(PointSamp, offsetUV, 0).xyz;
            
//            // 距離による棄却（ここでも worldPos と samplePos の不一致を許容し安定させる）
//            float distDiff = length(samplePos - worldPos);
//            float w = blurWeights5[y + 2] * blurWeights5[x + 2];
            
//            // 閾値をわずかに広げてジッターの微差を吸収
//            float weight = (distDiff < 0.6f) ? w : 0.0f;
            
//            ao += sampleAO * weight;
//            weightSum += weight;
//        }
//    }
//    ao = (weightSum > 0.0001f) ? (ao / weightSum) : 1.0f;

//    diffIBL *= ao;
//    specIBL *= lerp(1.0f, ao, 0.8f);

//    // --- [ 4. 最終色合成 ] ---
//    float3 color = Lo + diffIBL + specIBL;

//    // 影（安定した stableViewDepth を渡す）
//    float shadowFactor = CalcShadowFactorCSM_Stable(worldPos, stableViewDepth);
//    float3 shadow = lerp(shadowColor_CSM.rgb, float3(1.0f, 1.0f, 1.0f), shadowFactor);
//    color.rgb *= shadow;

//    return float4(color, 1.0f);
//}

// ==========================================
// DeferredLightingPS.hlsl (SSGI統合 ＆ 物理影修正 ＆ プローブ反射対応)
// ==========================================
#include "PBR.hlsli"
#include "ShadingFunctions.hlsli"

Texture2D GBuffer0 : register(t0);
Texture2D GBuffer1 : register(t1);
Texture2D GBuffer2 : register(t2);
Texture2D AOMap : register(t3);
Texture2DArray shadowMap : register(t4);

// 空間ブラー済みの極上SSGIバッファ等
Texture2D SSGIMap : register(t5);
Texture2D FogMap : register(t6);
Texture2D SSRMap : register(t7);

// ★追加: ローカル反射プローブ (In-Engine Bakerで撮影されたCubemap)
TextureCube ProbeMap : register(t8);

SamplerComparisonState shadowSampler : register(s1);
TextureCube diffuse_iem : register(t33);
TextureCube specular_pmrem : register(t34);
Texture2D lut_ggx : register(t35);

SamplerState PointSamp : register(s2);
SamplerState LinearSamp : register(s3);

static const float PI_INV = 1.0f / 3.14159265f;

// 影の計算（安定した viewDepth を受け取るように変更）
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
    // --- [ 1. G-Buffer サンプリング ] ---
    float4 g1 = GBuffer1.Sample(PointSamp, pin.uv);
    if (length(g1.xyz) < 0.1f)
        discard; // 背景なら描画しない

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

    // =========================================================
    // ★ 物理影: 影は「太陽光（平行光源）」の寄与にのみ掛ける
    // =========================================================
    float shadowFactor = CalcShadowFactorCSM_Stable(worldPos, stableViewDepth);
    float3 dirShadow = lerp(shadowColor_CSM.rgb, float3(1.0f, 1.0f, 1.0f), shadowFactor);

    // --- [ 2. 直接光 (Direct Lighting) ] ---
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
            
            // 太陽光の寄与にのみ影(dirShadow)を掛ける
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

    // --- [ 3. 環境光とAO (IBL & GTAO) ] ---
    float3 diffIBL = DiffuseIBL(N, -V, roughness, albedoLin * (1.0f - metallic), F0, diffuse_iem, LinearSamp);
    
    // ベースとなる空の反射 (Skybox IBL)
    float3 specIBL = SpecularIBL(N, -V, roughness, F0, lut_ggx, specular_pmrem, LinearSamp);

    // =========================================================
    // ★ 階層1: リフレクションプローブの合成 (空のIBLを上書き)
    // =========================================================
    float3 R = reflect(-V, N);
    float3 probeColor = ProbeMap.SampleLevel(LinearSamp, R, 0).rgb;
    
    // プローブが黒（未撮影や無効）でなければ、空の代わりにプローブを使う
    float probeWeight = saturate(dot(probeColor, float3(0.33f, 0.33f, 0.33f)) * 100.0f);
    
    // プローブの色にもフレネルとラフネスの減衰をかける（環境BRDF近似）
    float NdotV_probe = max(dot(N, V), 0.0f);
    float2 envBRDF = lut_ggx.SampleLevel(LinearSamp, float2(NdotV_probe, roughness), 0).rg;
    float3 probeSpec = probeColor * (F0 * envBRDF.x + envBRDF.y);
    
    specIBL = lerp(specIBL, probeSpec, probeWeight);

    // =========================================================
    // ★ 階層2: SSRの合成 (プローブをさらに上書き)
    // =========================================================
    float3 ssrColor = SSRMap.SampleLevel(LinearSamp, pin.uv, 0).rgb;
    
    // SSRの色が黒より明るければSSRを優先し、そうでなければプローブ(または空)の反射を使う
    float ssrWeight = saturate(dot(ssrColor, float3(0.33f, 0.33f, 0.33f)) * 10.0f);
    specIBL = lerp(specIBL, ssrColor, ssrWeight);
    
    
    // AO バイラテラルブラー
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
    specIBL *= lerp(1.0f, ao, 0.8f); // 鏡面反射に対するAOは少し弱める

    // =========================================================
    // ★ 4. SSGIの合成（間接ディフューズ光の加算）
    // =========================================================
    // ハーフ解像度のSSGIを滑らかに拡大するため LinearSamp を使用
    float3 ssgiColor = SSGIMap.SampleLevel(LinearSamp, pin.uv, 0).rgb;
    
    // SSGI はバウンスした間接光。表面色(Albedo)を乗算し、AOで隅の光漏れを防ぐ
    float3 indirectDiffuse = ssgiColor * albedoLin * (1.0f - metallic) * ao;

    // --- [ 5. 最終色合成 ] ---
    // 物理法則に基づき、すべての光（直接光 + IBL + SSGIバウンス光 + スペキュラ反射）を足し合わせる
    float3 color = Lo + diffIBL + indirectDiffuse + specIBL;

    
    float3 fogColor = FogMap.SampleLevel(LinearSamp, pin.uv, 0).rgb;
    color += fogColor;
    
    
    return float4(color, 1.0f);
}

