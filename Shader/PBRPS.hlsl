#include "PBR.hlsli"
#include "ShadingFunctions.hlsli"

// SRV を作成する。 と Sampler のバインド定義。
Texture2D AlbedoMap : register(t0);
Texture2D NormalMap : register(t1);
Texture2D MRMap : register(t2);
// G チャンネルを roughness、B チャンネルを metallic として扱う。
Texture2D OcclMap : register(t3);
Texture2DArray shadowMap : register(t4);
SamplerComparisonState shadowSampler : register(s1);
TextureCube diffuse_iem : register(t33);
TextureCube specular_pmrem : register(t34);
Texture2D lut_ggx : register(t35);
SamplerState LinearSamp : register(s0);

/* ---------- 定数 ---------- */
static const float GAMMA = 2.2f;
static const float INV_GAMMA = 1.0f / GAMMA;
static const float PI_INV = 1.0f / PI;

// カスケードシャドウを計算する。
float CalcShadowFactorCSM(float3 worldPos, float viewDepth)
{
    uint cascadeIndex = 0;
    if (viewDepth > cascadeSplits.x)
        cascadeIndex = 1;
    if (viewDepth > cascadeSplits.y)
        cascadeIndex = 2;
    if (viewDepth > cascadeSplits.z)
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

float4 main(VS_OUT pin) : SV_TARGET
{
    float3 albedoLin = pow(AlbedoMap.Sample(LinearSamp, pin.texcoord).rgb, GAMMA);
    albedoLin *= materialColor.rgb;

    float2 rm = MRMap.Sample(LinearSamp, pin.texcoord).gb;
    float roughness = clamp(rm.x * roughnessFactor, 0.05f, 1.0f);
    float metallic = saturate(rm.y * metallicFactor);

    float aoSample = OcclMap.Sample(LinearSamp, pin.texcoord).r;
    float ao = lerp(1.0f, aoSample, occlusionStrength);

    float3 N = normalize(pin.normal);
    float3 T = normalize(pin.tangent);
    T = normalize(T - N * dot(N, T));
    float3 B = normalize(cross(N, T));
    float3 nMap = NormalMap.Sample(LinearSamp, pin.texcoord).xyz * 2.0f - 1.0f;
    N = normalize(nMap.x * T + nMap.y * B + nMap.z * N);

    float3 V = normalize(cameraPosition.xyz - pin.position);

    float3 F0 = lerp(float3(0.04, 0.04, 0.04), albedoLin, metallic);

    float3 Lo = float3(0, 0, 0);

    {
        float3 L = normalize(-lightDirection.xyz);
        float3 H = normalize(V + L);
        
        float NdotL = max(dot(N, L), 0.0);
        float NdotV = max(dot(N, V), 1e-4);
        float NdotH = max(dot(N, H), 0.0);
        float VdotH = max(dot(V, H), 0.0);

        if (NdotL > 0.0)
        {
            float3 F = CalcFresnel(F0, VdotH);
            float D = CalcNormalDistributionFunction(NdotH, roughness);
            float G = CalcGeometryFunction(NdotL, NdotV, roughness);

            float3 Spec = (D * G * F) / max(4.0f * NdotL * NdotV, 1e-4);
            float3 Diff = (1.0f - F) * (1.0f - metallic) * albedoLin * PI_INV;

            float3 radiance = lightColor.rgb;
            Lo += (Diff + Spec) * radiance * NdotL;
        }
    }

    for (int i = 0; i < (int) pointLightCount; ++i)
    {
        PointLight light = pointLights[i];
        float3 L_vec = light.position - pin.position;
        float dist = length(L_vec);
        if (dist >= light.range)
            continue;

        float3 L = normalize(L_vec);
        float3 H = normalize(V + L);
        float NdotL = max(dot(N, L), 0.0);
        float NdotV = max(dot(N, V), 1e-4);
        float NdotH = max(dot(N, H), 0.0);
        float VdotH = max(dot(V, H), 0.0);

        if (NdotL > 0.0)
        {
            float attenuation = saturate(1.0 - (dist / light.range));
            attenuation *= attenuation;
            float3 F = CalcFresnel(F0, VdotH);
            float D = CalcNormalDistributionFunction(NdotH, roughness);
            float G = CalcGeometryFunction(NdotL, NdotV, roughness);
            float3 Spec = (D * G * F) / max(4.0f * NdotL * NdotV, 1e-4);
            float3 Diff = (1.0f - F) * (1.0f - metallic) * albedoLin * PI_INV;
            Lo += (Diff + Spec) * (light.color * light.intensity * attenuation) * NdotL;
        }
    }

    float3 diffIBL = DiffuseIBL(N, -V, roughness, albedoLin * (1.0f - metallic), F0, diffuse_iem, LinearSamp);
    float3 specIBL = SpecularIBL(N, -V, roughness, F0, lut_ggx, specular_pmrem, LinearSamp);

    float3 color = Lo + diffIBL + specIBL;
    
    color += albedoLin * emissiveFactor;
    
    color = lerp(float3(0.03, 0.03, 0.03), color, ao);

    float shadowFactor = CalcShadowFactorCSM(pin.position, pin.viewDepth);
    float3 shadow = lerp(shadowColor_CSM.rgb, float3(1.0f, 1.0f, 1.0f), shadowFactor);
    color.rgb *= shadow;

    return float4(pow(color, INV_GAMMA), 1.0f);
}
