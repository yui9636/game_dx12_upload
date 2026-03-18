#include "Phong.hlsli"
#include"Skinning.hlsli"

VS_OUT main(
float4 position:POSITION,
float4 boneWeights:BONE_WEIGHTS,
uint4 boneIndices:BONE_INDICES,
float2 texcoord:TEXCOORD,
float3 normal:NORMAL,
float3 tangent:TANGENT
)
{
    VS_OUT vout = (VS_OUT) 0;
    position = SkinningPosition(position, boneWeights, boneIndices);
    vout.vertex = mul(position, viewProjection);
    vout.texcoord = texcoord;
    vout.normal = SkinningVector(normal, boneWeights, boneIndices);
    vout.position = position.xyz;
    vout.tangent = SkinningVector(tangent, boneWeights, boneIndices);
    
    float4 shadow = mul(position, lightViewProjection);
    shadow.xyz /= shadow.w;
    shadow.y = -shadow.y;
    shadow.xy = shadow.xy * 0.5f + 0.5f;
    vout.shadow = shadow.xyz;
   
    return vout;
}
