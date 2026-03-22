#include "Skinning.hlsli"
#include "PBR.hlsli"

VS_OUT main(
float4 position : POSITION,
float4 boneWeights : BONE_WEIGHTS,
uint4 boneIndices : BONE_INDICES,
float2 texcoord : TEXCOORD,
float3 normal : NORMAL,
float3 tangent : TANGENT,
float4 instanceWorld0 : INSTANCE_WORLD0,
float4 instanceWorld1 : INSTANCE_WORLD1,
float4 instanceWorld2 : INSTANCE_WORLD2,
float4 instanceWorld3 : INSTANCE_WORLD3,
float4 instancePrevWorld0 : INSTANCE_PREV_WORLD0,
float4 instancePrevWorld1 : INSTANCE_PREV_WORLD1,
float4 instancePrevWorld2 : INSTANCE_PREV_WORLD2,
float4 instancePrevWorld3 : INSTANCE_PREV_WORLD3
)
{
    VS_OUT vout = (VS_OUT)0;

    float4x4 instanceWorld = float4x4(
        instanceWorld0,
        instanceWorld1,
        instanceWorld2,
        instanceWorld3);

    float4x4 instancePrevWorld = float4x4(
        instancePrevWorld0,
        instancePrevWorld1,
        instancePrevWorld2,
        instancePrevWorld3);

    // boneTransforms[0] にノードローカル変換が入っている（非スキンメッシュ用）
    float4 localPos = mul(position, boneTransforms[0]);

    float4 worldPosition = mul(localPos, instanceWorld);
    vout.vertex = mul(worldPosition, viewProjection);
    vout.texcoord = texcoord;
    float3 localNormal = mul(float4(normal, 0.0f), boneTransforms[0]).xyz;
    vout.normal = normalize(mul(float4(localNormal, 0.0f), instanceWorld).xyz);
    vout.position = worldPosition.xyz;
    float3 localTangent = mul(float4(tangent, 0.0f), boneTransforms[0]).xyz;
    vout.tangent = normalize(mul(float4(localTangent, 0.0f), instanceWorld).xyz);
    vout.viewDepth = vout.vertex.w;

    float4 curPosUnjittered = mul(worldPosition, viewProjectionUnjittered);
    vout.curClipPos = curPosUnjittered;

    float4 prevLocalPos = mul(position, prevBoneTransforms[0]);
    float4 prevWorldPosition = mul(prevLocalPos, instancePrevWorld);
    vout.prevClipPos = mul(prevWorldPosition, prevViewProjection);

    return vout;
}
