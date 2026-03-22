#include "Skinning.hlsli"

cbuffer CbScene : register(b0)
{
    row_major float4x4 lightViewProjection;
}

float4 main(
    float4 position : POSITION,
    float4 boneWeights : BONE_WEIGHTS,
    uint4 boneIndices : BONE_INDICES,
    float4 instanceWorld0 : INSTANCE_WORLD0,
    float4 instanceWorld1 : INSTANCE_WORLD1,
    float4 instanceWorld2 : INSTANCE_WORLD2,
    float4 instanceWorld3 : INSTANCE_WORLD3)
    : SV_POSITION
{
    float4x4 instanceWorld = float4x4(
        instanceWorld0,
        instanceWorld1,
        instanceWorld2,
        instanceWorld3);

    float4 localPos = mul(position, boneTransforms[0]);
    float4 worldPosition = mul(localPos, instanceWorld);
    return mul(worldPosition, lightViewProjection);
}
