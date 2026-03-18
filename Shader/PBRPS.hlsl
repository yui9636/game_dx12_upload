//#include "PBR.hlsli"
//#include "ShadingFunctions.hlsli"
///* ---------- SRV & Sampler (�M�a�̒�`����S�ێ�) ---------- */
//Texture2D AlbedoMap : register(t0);
//Texture2D NormalMap : register(t1);
//Texture2D MRMap : register(t2);
//// G = roughness , B = metallic
//Texture2D OcclMap : register(t3);
//// ��CSM�Ή��̂��� Texture2DArray �Ɍ^�ύX
//Texture2DArray shadowMap : register(t4);
//SamplerComparisonState shadowSampler : register(s1);
//TextureCube diffuse_iem : register(t33);
//TextureCube specular_pmrem : register(t34);
//Texture2D lut_ggx : register(t35);
//SamplerState LinearSamp : register(s0);
///* ---------- �萔 ---------- */
//static const float GAMMA = 2.2f;
//static const float INV_GAMMA = 1.0f / GAMMA;
//static const float PI_INV = 1.0f / PI;
//// --------------------------------------------------------
//// ���ǉ�: �J�X�P�[�h�V���h�E�v�Z�֐�
//// (CbShadowMap �̃����o cascadeSplits, lightViewProjections ���̓w�b�_�[����Q��)
//// --------------------------------------------------------
//float CalcShadowFactorCSM(float3 worldPos, float viewDepth)
//{
//    // 1. �[�x�Ɋ�Â��ăJ�X�P�[�h��I��
//    uint cascadeIndex = 0;
//    if (viewDepth > cascadeSplits.x)
//        cascadeIndex = 1;
//    if (viewDepth > cascadeSplits.y)
//        cascadeIndex = 2;
//    if (viewDepth > cascadeSplits.z)
//        return 1.0f;
//    // 2. ���C�g��Ԃւ̍��W�ϊ�
//    float4 lightPos = mul(float4(worldPos, 1.0f), lightViewProjections[cascadeIndex]);
//    float3 projCoords = lightPos.xyz / lightPos.w;
//    projCoords.x = projCoords.x * 0.5f + 0.5f;
//    projCoords.y = -projCoords.y * 0.5f + 0.5f;
//    // �͈͊O�`�F�b�N
//    if (projCoords.x < 0.0f || projCoords.x > 1.0f || projCoords.y < 0.0f || projCoords.y > 1.0f || projCoords.z > 1.0f)
//        return 1.0f;
//    // 3. PCF�T���v�����O (3x3)
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
//float4 main(VS_OUT pin) : SV_TARGET
//{
//    /* --- �@ �}�e���A���p�����[�^ (�M�a�̎�����ێ�) --- */
//    float3 albedoLin = pow(AlbedoMap.Sample(LinearSamp, pin.texcoord).rgb, GAMMA);
//    albedoLin *= materialColor.rgb;
//    float2 rm = MRMap.Sample(LinearSamp, pin.texcoord).gb;
//    float roughness = clamp(rm.x * roughnessFactor, 0.05f, 1.0f);
//    float metallic = saturate(rm.y * metallicFactor);
//    float aoSample = OcclMap.Sample(LinearSamp, pin.texcoord).r;
//    float ao = lerp(1.0f, aoSample, occlusionStrength);
//    /* --- �A �@���iTBN�j (�M�a�̎�����ێ�) --- */
//    float3 N = normalize(pin.normal);
//    float3 T = normalize(pin.tangent);
//    T = normalize(T - N * dot(N, T));
//    float3 B = normalize(cross(N, T));
//    float3 nMap = NormalMap.Sample(LinearSamp, pin.texcoord).xyz * 2.0f - 1.0f;
//    N = normalize(nMap.x * T + nMap.y * B + nMap.z * N);
//    /* --- �B �����x�N�g�� --- */
//    float3 V = normalize(cameraPosition.xyz - pin.position);
//    /* --- �C Fresnel F0 --- */
//    float3 F0 = lerp(float3(0.04, 0.04, 0.04), albedoLin, metallic);
//    // ���C�e�B���O�~��
//    float3 Lo = float3(0, 0, 0);
//    // ==========================================
//    // A. �f�B���N�V���i�����C�g (���z��)
//    // ==========================================
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
//            float3 radiance = lightColor.rgb;
//            Lo += (Diff + Spec) * radiance * NdotL;
//        }
//    }
//    // ==========================================
//    // B. �|�C���g���C�g (�M�a�̃��[�v�v�Z��ێ�)
//    // ==========================================
//    for (int i = 0; i < (int) pointLightCount; ++i)
//    {
//        PointLight light = pointLights[i];
//        float3 L_vec = light.position - pin.position;
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
//    /* ---------- IBL (����) (�M�a�̎�����ێ�) ---------- */
//    float3 diffIBL = DiffuseIBL(N, -V, roughness, albedoLin * (1.0f - metallic), F0, diffuse_iem, LinearSamp);
//    float3 specIBL = SpecularIBL(N, -V, roughness, F0, lut_ggx, specular_pmrem, LinearSamp);
//    /* --- �D ���� & AO ----------------------------------------------- */
//    float3 color = Lo + diffIBL + specIBL;
//    color = lerp(float3(0.03, 0.03, 0.03), color, ao);
//    /* --- �e (Cascade Shadow Mapping) --- */
//    // ���ŏ��ύX: �M�a�̉e�K�p���W�b�N��CSM�p�ɍ����ւ�
//    // pin.viewDepth �� VS_OUT �� LinearDepth ��n���Ă���z��
//    float shadowFactor = CalcShadowFactorCSM(pin.position, pin.viewDepth);
//    // shadowColor_CSM �� CbShadowMap �R��
//    float3 shadow = lerp(shadowColor_CSM.rgb, float3(1.0f, 1.0f, 1.0f), shadowFactor);
//    color.rgb *= shadow;
//    return float4(pow(color, INV_GAMMA), 1.0f);
//}
#include "PBR.hlsli"
#include "ShadingFunctions.hlsli"

/* ---------- SRV & Sampler ---------- */
Texture2D AlbedoMap : register(t0);
Texture2D NormalMap : register(t1);
Texture2D MRMap : register(t2);
// G = roughness , B = metallic
Texture2D OcclMap : register(t3);
Texture2DArray shadowMap : register(t4);
SamplerComparisonState shadowSampler : register(s1);
TextureCube diffuse_iem : register(t33);
TextureCube specular_pmrem : register(t34);
Texture2D lut_ggx : register(t35);
SamplerState LinearSamp : register(s0);

/* ---------- �萔 ---------- */
static const float GAMMA = 2.2f;
static const float INV_GAMMA = 1.0f / GAMMA;
static const float PI_INV = 1.0f / PI;

// --------------------------------------------------------
// �J�X�P�[�h�V���h�E�v�Z�֐�
// --------------------------------------------------------
float CalcShadowFactorCSM(float3 worldPos, float viewDepth)
{
    // 1. �[�x�Ɋ�Â��ăJ�X�P�[�h��I��
    uint cascadeIndex = 0;
    if (viewDepth > cascadeSplits.x)
        cascadeIndex = 1;
    if (viewDepth > cascadeSplits.y)
        cascadeIndex = 2;
    if (viewDepth > cascadeSplits.z)
        return 1.0f;

    // 2. ���C�g��Ԃւ̍��W�ϊ�
    float4 lightPos = mul(float4(worldPos, 1.0f), lightViewProjections[cascadeIndex]);
    float3 projCoords = lightPos.xyz / lightPos.w;
    projCoords.x = projCoords.x * 0.5f + 0.5f;
    projCoords.y = -projCoords.y * 0.5f + 0.5f;

    // �͈͊O�`�F�b�N
    if (projCoords.x < 0.0f || projCoords.x > 1.0f || projCoords.y < 0.0f || projCoords.y > 1.0f || projCoords.z > 1.0f)
        return 1.0f;

    // 3. PCF�T���v�����O (3x3)
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
    /* --- �@ �}�e���A���p�����[�^ --- */
    float3 albedoLin = pow(AlbedoMap.Sample(LinearSamp, pin.texcoord).rgb, GAMMA);
    albedoLin *= materialColor.rgb;

    float2 rm = MRMap.Sample(LinearSamp, pin.texcoord).gb;
    float roughness = clamp(rm.x * roughnessFactor, 0.05f, 1.0f);
    float metallic = saturate(rm.y * metallicFactor);

    float aoSample = OcclMap.Sample(LinearSamp, pin.texcoord).r;
    float ao = lerp(1.0f, aoSample, occlusionStrength);

    /* --- �A �@���iTBN�j --- */
    float3 N = normalize(pin.normal);
    float3 T = normalize(pin.tangent);
    T = normalize(T - N * dot(N, T));
    float3 B = normalize(cross(N, T));
    float3 nMap = NormalMap.Sample(LinearSamp, pin.texcoord).xyz * 2.0f - 1.0f;
    N = normalize(nMap.x * T + nMap.y * B + nMap.z * N);

    /* --- �B ����x�N�g�� --- */
    float3 V = normalize(cameraPosition.xyz - pin.position);

    /* --- �C Fresnel F0 --- */
    float3 F0 = lerp(float3(0.04, 0.04, 0.04), albedoLin, metallic);

    // ���C�e�B���O�~��
    float3 Lo = float3(0, 0, 0);

    // ==========================================
    // A. �f�B���N�V���i�����C�g (���z��)
    // ==========================================
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

    // ==========================================
    // B. �|�C���g���C�g
    // ==========================================
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

    /* ---------- IBL (����) ---------- */
    float3 diffIBL = DiffuseIBL(N, -V, roughness, albedoLin * (1.0f - metallic), F0, diffuse_iem, LinearSamp);
    float3 specIBL = SpecularIBL(N, -V, roughness, F0, lut_ggx, specular_pmrem, LinearSamp);

    /* --- �D ���� & AO & ���� ----------------------------------------------- */
    float3 color = Lo + diffIBL + specIBL;
    
    // ���ǉ�: ���������i�A���x�h�F �~ �������x�j����Z
    color += albedoLin * emissiveFactor;
    
    color = lerp(float3(0.03, 0.03, 0.03), color, ao);

    /* --- �e (Cascade Shadow Mapping) --- */
    float shadowFactor = CalcShadowFactorCSM(pin.position, pin.viewDepth);
    float3 shadow = lerp(shadowColor_CSM.rgb, float3(1.0f, 1.0f, 1.0f), shadowFactor);
    color.rgb *= shadow;

    return float4(pow(color, INV_GAMMA), 1.0f);
}