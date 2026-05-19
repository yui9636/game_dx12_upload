#include "Toon.hlsli"

Texture2D    diffuseMap     : register(t0);
Texture2D    normalMap      : register(t1);
Texture2D    shadowMap      : register(t2);
Texture2D    rampMap        : register(t3);
SamplerState linearSampler  : register(s0);
SamplerComparisonState shadowSampler : register(s1);

float4 main(VS_OUT pin) : SV_TARGET
{
    // Base color
    float4 base = diffuseMap.Sample(linearSampler, pin.texcoord) * materialColor;

    // Normal mapping
    float3 N = normalize(pin.normal);
    float3 T = normalize(pin.tangent);
    float3 B = normalize(cross(N, T));
    float3 nm = normalMap.Sample(linearSampler, pin.texcoord).xyz * 2.0f - 1.0f;
    N = normalize(nm.x * T + nm.y * B + nm.z * N);

    // Light direction (per-material fixed or world)
    float3 L;
    if (specParams.z > 0.5f) {
        L = normalize(-fixedLight.xyz);
    } else {
        L = normalize(-lightDirection.xyz);
    }

    float3 V = normalize(cameraPosition.xyz - pin.position);

    // Half-lambert NdotL for softer shading
    float NdotL = saturate(dot(N, L) * 0.5f + 0.5f);

    // Shading mode: 0=Bands, 1=ThreeTier, 2=Ramp
    int mode = (int)shadowParams.w;
    float3 litColor;

    if (mode == 2) {
        // Ramp texture
        litColor = rampMap.Sample(linearSampler, float2(NdotL, 0.5f)).rgb;
    } else if (mode == 1) {
        // Three-tier: lit > shadowMid > shadowDeep
        float midT = shadowParams.x;
        float deepT = shadowParams.y;
        if (NdotL >= midT) {
            litColor = lightColor.rgb;
        } else if (NdotL >= deepT) {
            litColor = shadowMid.rgb;
        } else {
            litColor = shadowDeep.rgb;
        }
    } else {
        // Bands (quantized)
        float bandLevels = max(2.0f, shadowParams.z);
        float quant = floor(NdotL * bandLevels) / (bandLevels - 1.0f);
        quant = saturate(quant);
        litColor = lerp(shadowMid.rgb, lightColor.rgb, quant);
    }

    float3 color = base.rgb * litColor;

    // Banded specular (Blinn-Phong NdotH, thresholded)
    if (specColor.a > 0.001f) {
        float3 H = normalize(L + V);
        float NdotH = saturate(dot(N, H));
        // sharpness in [0,1] -> exponent 4..256
        float exponent = lerp(4.0f, 256.0f, saturate(specParams.x));
        float spec = pow(NdotH, exponent);
        spec = step(specParams.y, spec);  // hard cutoff
        color += spec * specColor.rgb * specColor.a;
    }

    // Anisotropic strip (simple Kajiya-Kay along tangent)
    if (specParams.w > 0.5f) {
        float3 Tn = T;
        // Shift tangent along binormal by anisoOffset
        Tn = normalize(Tn + B * fixedLight.w);
        float3 H = normalize(L + V);
        float TdotH = dot(Tn, H);
        float sinTH = sqrt(saturate(1.0f - TdotH * TdotH));
        float aniso = pow(sinTH, lerp(8.0f, 256.0f, saturate(anisoAux.x)));
        aniso = step(0.55f, aniso);
        color += aniso * specColor.rgb * 0.6f;
    }

    // Rim light
    float rim = pow(1.0f - saturate(dot(N, V)), max(0.5f, rimColor.a));
    color += rim * rimColor.rgb * rimAux.x;

    // Shadow map PCF (3x3)
    const float shadowBias = 0.001f;
    float shadowFactor = 0.0f;
    const float2 offsets[9] = {
        float2(-shadowTexelSize, -shadowTexelSize),
        float2(0.0f,              -shadowTexelSize),
        float2(shadowTexelSize,  -shadowTexelSize),
        float2(-shadowTexelSize, 0.0f),
        float2(0.0f,              0.0f),
        float2(shadowTexelSize,  0.0f),
        float2(-shadowTexelSize, shadowTexelSize),
        float2(0.0f,              shadowTexelSize),
        float2(shadowTexelSize,  shadowTexelSize),
    };
    [unroll]
    for (int i = 0; i < 9; ++i) {
        shadowFactor += shadowMap.SampleCmpLevelZero(shadowSampler,
            pin.shadow.xy + offsets[i], pin.shadow.z - shadowBias).r;
    }
    shadowFactor /= 9.0f;

    float3 shadowTerm = lerp(shadowColor.rgb, float3(1, 1, 1), shadowFactor);
    color *= shadowTerm;

    return float4(color, base.a);
}
