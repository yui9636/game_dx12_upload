cbuffer TerrainCB : register(b0)
{
    float4x4 viewProj;
    float4x4 viewProjectionUnjittered;
    float4x4 prevViewProjection;
    float4   chunkOffset;
    float    heightScale;
    float3   pad;
};

struct VS_INPUT
{
    float3 position : POSITION;
    float3 normal   : NORMAL;
    float2 uv       : TEXCOORD0;
};

struct VS_OUTPUT
{
    float4 position    : SV_Position;
    float3 worldPos    : TEXCOORD0;
    float3 normal      : TEXCOORD1;
    float2 uv          : TEXCOORD2;
    float4 curClipPos  : TEXCOORD3;
    float4 prevClipPos : TEXCOORD4;
};

VS_OUTPUT main(VS_INPUT input)
{
    VS_OUTPUT output;
    float3 worldPos = input.position + chunkOffset.xyz;

    output.position    = mul(float4(worldPos, 1.0f), viewProj);
    output.worldPos    = worldPos;
    output.normal      = normalize(input.normal);
    output.uv          = input.uv;
    output.curClipPos  = mul(float4(worldPos, 1.0f), viewProjectionUnjittered);
    output.prevClipPos = mul(float4(worldPos, 1.0f), prevViewProjection);
    return output;
}
