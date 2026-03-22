#include "Phong.hlsli"

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
float4 instanceWorld3 : INSTANCE_WORLD3
)
{
    VS_OUT vout = (VS_OUT)0;

    float4x4 instanceWorld = float4x4(
        instanceWorld0,
        instanceWorld1,
        instanceWorld2,
        instanceWorld3);

    float4 worldPosition = mul(position, instanceWorld);
    vout.vertex = mul(worldPosition, viewProjection);
    vout.texcoord = texcoord;
    vout.normal = normalize(mul(float4(normal, 0.0f), instanceWorld).xyz);
    vout.position = worldPosition.xyz;
    vout.tangent = normalize(mul(float4(tangent, 0.0f), instanceWorld).xyz);

    float4 shadow = mul(worldPosition, lightViewProjection);
    shadow.xyz /= shadow.w;
    shadow.y = -shadow.y;
    shadow.xy = shadow.xy * 0.5f + 0.5f;
    vout.shadow = shadow.xyz;

    return vout;
}
