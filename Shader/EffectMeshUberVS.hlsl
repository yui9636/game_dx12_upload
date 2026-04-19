// EffectMeshUberVS — Phase B Route B
// Skinning + worldMatrix transform → ubershader PS_IN layout.
// Route A 移行時: particle_data/header buffer を読む別 VS に差し替える (PSO だけ入替)。

#include "Skinning.hlsli"

cbuffer CbScene : register(b7)
{
    row_major float4x4 viewProjection;
    row_major float4x4 viewProjectionUnjittered;
    row_major float4x4 prevViewProjection;
    float4 lightDirection;
    float4 lightColor;
    float4 cameraPosition;
    row_major float4x4 lightViewProjection;
    float4 shadowColor;
    float shadowTexelSize;
    float jitterX;
    float jitterY;
    float renderW;
    float renderH;
    float pointLightCount;
    float prevJitterX;
    float prevJitterY;
    float4 _cbScenePad[48];
};

struct VS_OUT
{
    float4 position     : SV_POSITION;
    float2 texcoord     : TEXCOORD0;
    float4 color        : COLOR0;
    float3 worldNormal  : TEXCOORD1;
    float3 worldTangent : TEXCOORD2;
    float3 worldPos     : TEXCOORD3;
};

VS_OUT main(
    float4 position    : POSITION,
    float4 boneWeights : BONE_WEIGHTS,
    uint4  boneIndices : BONE_INDICES,
    float2 texcoord    : TEXCOORD,
    float3 normal      : NORMAL,
    float3 tangent     : TANGENT)
{
    VS_OUT o = (VS_OUT)0;

    float4 worldPos4 = SkinningPosition(position, boneWeights, boneIndices);
    o.position     = mul(worldPos4, viewProjection);
    o.worldPos     = worldPos4.xyz;
    o.texcoord     = texcoord;
    o.worldNormal  = normalize(SkinningVector(normal,  boneWeights, boneIndices));
    o.worldTangent = normalize(SkinningVector(tangent, boneWeights, boneIndices));
    o.color        = float4(1.0f, 1.0f, 1.0f, 1.0f);
    return o;
}
