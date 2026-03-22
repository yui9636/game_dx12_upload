struct VS_OUT
{
	float4 vertex : SV_POSITION;
    float2 texcoord : TEXCOORD;
    float3 normal : NORMAL;
    float3 position : POSITION;
    float3 tangent : TANGENT;
    float3 shadow : SHADOW;
};

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
};

cbuffer CbMesh : register(b1)
{
    float4   materialColor;
};