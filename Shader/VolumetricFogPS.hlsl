// ==========================================
// VolumetricFogPS.hlsl
// ==========================================
#include "FullScreenQuad.hlsli"

Texture2D GBuffer2 : register(t0);
Texture2DArray shadowMap : register(t4);

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

float ComputeHenyeyGreenstein(float g, float cosTheta)
{
    float g2 = g * g;
    float num = 1.0f - g2;
    float denom = 1.0f + g2 - 2.0f * g * cosTheta;
    return num / (4.0f * 3.14159265f * pow(denom, 1.5f));
}

float GetShadowFactor(float3 worldPos, float viewDepth)
{
    uint cascadeIndex = 0;
    if (viewDepth > cascadeSplits.x) cascadeIndex = 1;
    if (viewDepth > cascadeSplits.y) cascadeIndex = 2;
    if (viewDepth > cascadeSplits.z) return 1.0f;

    float4 lightPos = mul(float4(worldPos, 1.0f), lightViewProjections[cascadeIndex]);
    float3 projCoords = lightPos.xyz / lightPos.w;
    projCoords.x = projCoords.x * 0.5f + 0.5f;
    projCoords.y = -projCoords.y * 0.5f + 0.5f;

    if (projCoords.x < 0.0f || projCoords.x > 1.0f ||
        projCoords.y < 0.0f || projCoords.y > 1.0f ||
        projCoords.z > 1.0f)
    {
        return 1.0f;
    }

    float currentDepth = projCoords.z - shadowBias_CSM.x;
    float shadow = 0.0f;
    const float2 texelSize = float2(1.0f / 4096.0f, 1.0f / 4096.0f);

    [unroll]
    for (int x = -1; x <= 1; ++x)
    {
        [unroll]
        for (int y = -1; y <= 1; ++y)
        {
            float3 uvw = float3(projCoords.xy + float2(x, y) * texelSize, (float)cascadeIndex);
            shadow += shadowMap.SampleCmpLevelZero(shadowSampler, uvw, currentDepth);
        }
    }
    return shadow / 9.0f;
}

float rand(float2 uv)
{
    return frac(sin(dot(uv, float2(12.9898, 78.233))) * 43758.5453);
}

float4 main(VS_OUT pin) : SV_TARGET
{
    float4 wpd = GBuffer2.Sample(pointSampler, pin.texcoord);
    float3 targetWorldPos = wpd.xyz;

    float3 startPos = cameraPosition.xyz;
    float3 rayVec = targetWorldPos - startPos;

    float maxDistance = 50.0f;
    float distToTarget = length(rayVec);
    float rayLength = min(distToTarget, maxDistance);
    float3 rayDir = rayVec / max(distToTarget, 1e-4f);

    const int STEP_COUNT = 16;
    float stepSize = rayLength / (float)STEP_COUNT;
    float dither = rand(pin.texcoord + jitterX) * stepSize;
    float3 currentPos = startPos + rayDir * dither;

    float3 accumulatedFog = float3(0.0f, 0.0f, 0.0f);
    float3 L_dir = normalize(-lightDirection.xyz);
    float cosTheta = dot(rayDir, L_dir);
    float phaseDirectional = ComputeHenyeyGreenstein(0.7f, cosTheta);
    float scatteringCoeff = 0.05f;

    for (int i = 0; i < STEP_COUNT; ++i)
    {
        float currentViewDepth = length(currentPos - startPos);
        float shadowFactor = GetShadowFactor(currentPos, currentViewDepth);
        float3 directionalLightInscattering = lightColor.rgb * shadowFactor * phaseDirectional;

        float3 pointLightInscattering = float3(0.0f, 0.0f, 0.0f);
        for (int p = 0; p < (int)pointLightCount; ++p)
        {
            float3 L_vec = pointLights[p].position - currentPos;
            float dist = length(L_vec);
            if (dist < pointLights[p].range)
            {
                float attenuation = saturate(1.0f - (dist / pointLights[p].range));
                attenuation *= attenuation;

                float3 pL_dir = normalize(L_vec);
                float pCosTheta = dot(rayDir, pL_dir);
                float pPhase = ComputeHenyeyGreenstein(0.2f, pCosTheta);
                pointLightInscattering += pointLights[p].color * pointLights[p].intensity * attenuation * pPhase;
            }
        }

        accumulatedFog += (directionalLightInscattering + pointLightInscattering) * scatteringCoeff * stepSize;
        currentPos += rayDir * stepSize;
    }

    return float4(accumulatedFog, 1.0f);
}
