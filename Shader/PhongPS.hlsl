#include "Phong.hlsli"

Texture2D diffuseMap : register(t0);
SamplerState linearSampler : register(s0);
Texture2D normalMap : register(t1);
Texture2D shadowMap : register(t2);
SamplerComparisonState shadowSampler : register(s1);


float4 main(VS_OUT pin):SV_TARGET
{
    float4 color = diffuseMap.Sample(linearSampler, pin.texcoord) * materialColor;

    float3 N = normalize(pin.normal);
    float3 T = normalize(pin.tangent);
    float3 B = normalize(cross(N, T));

    float3 normal = normalMap.Sample(linearSampler, pin.texcoord).xyz;
    normal = (normal * 2.0f) - 1.0f;
    N = normalize((normal.x * T) + (normal.y * B) + (normal.z * N));


    float3 L = normalize(-lightDirection.xyz);
    float power = max(0, dot(L, N));
    power = power * 0.7 + 0.3f;

    color.rgb *= lightColor.rgb * power;

    float3 V = normalize(cameraPosition.xyz - pin.position);
    float specular = pow(max(0, dot(N, normalize(V + L))), 128);
    color.rgb += specular;

    const float shadowBias = 0.001f;
    float shadowFactor = 0.0f;
    const float2 shadowTexelOffsets[9] =
    {
        float2(-shadowTexelSize, -shadowTexelSize),// 左上
        float2(0.0f, -shadowTexelSize),              // 上
        float2(shadowTexelSize, -shadowTexelSize), // 右上
        float2(-shadowTexelSize, 0.0f),              // 左
        float2(0.0f, 0.0f),                          // 中
        float2(shadowTexelSize, 0.0f),               // 右
        float2(-shadowTexelSize, shadowTexelSize), // 左下
        float2(0.0f, shadowTexelSize),               // 下
        float2(shadowTexelSize, shadowTexelSize)   // 右下
        };

    [unroll]
    for (int i = 0; i < 9; ++i)
    {
        shadowFactor += shadowMap.SampleCmpLevelZero(shadowSampler, pin.shadow.xy + shadowTexelOffsets[i], pin.shadow.z - shadowBias).r;
    }
    shadowFactor /= 9.0f;

    float3 shadow = lerp(shadowColor.rgb, float3(1.0f, 1.0f, 1.0f), shadowFactor);
    color.rgb *= shadow;

    return color;

}
