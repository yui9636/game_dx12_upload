#include"Skinning.hlsli"
#include "PBR.hlsli"

VS_OUT main(
float4 position : POSITION,
float4 boneWeights : BONE_WEIGHTS,
uint4 boneIndices : BONE_INDICES,
float2 texcoord : TEXCOORD,
float3 normal : NORMAL,
float3 tangent : TANGENT
)
{
    VS_OUT vout = (VS_OUT) 0;
    
    float4 origPosition = position;
    
    position = SkinningPosition(position, boneWeights, boneIndices);
    vout.vertex = mul(position, viewProjection);
    vout.texcoord = texcoord;
    vout.normal = SkinningVector(normal, boneWeights, boneIndices);
    vout.position = position.xyz;
    vout.tangent = SkinningVector(tangent, boneWeights, boneIndices);
    vout.viewDepth = vout.vertex.w;
   
    float4 curPosUnjittered = mul(position, viewProjectionUnjittered);
    vout.curClipPos = curPosUnjittered;

    float4 prevPos = PrevSkinningPosition(origPosition, boneWeights, boneIndices);
    vout.prevClipPos = mul(prevPos, prevViewProjection);
    
    
    
    return vout;
}
