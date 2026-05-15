cbuffer TerrainCB : register(b0)
{
    float4x4 viewProj;                 // jittered view-projection (現フレーム)
    float4x4 viewProjectionUnjittered; // ジッタなし VP (Velocity 計算用)
    float4x4 prevViewProjection;       // 前フレーム VP (Velocity 計算用)
    float4   chunkOffset;              // xyz = world offset
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
    float4 curClipPos  : TEXCOORD3;    // ジッタなし現フレームクリップ座標
    float4 prevClipPos : TEXCOORD4;    // 前フレームクリップ座標
};

VS_OUTPUT main(VS_INPUT input)
{
    VS_OUTPUT output;
    float3 worldPos  = input.position + chunkOffset.xyz;
    output.position    = mul(float4(worldPos, 1.0f), viewProj);
    output.worldPos    = worldPos;
    output.normal      = normalize(input.normal);
    output.uv          = input.uv;
    // Velocity 計算用: ジッタなし VP と prev VP に同じワールド座標を流す。
    output.curClipPos  = mul(float4(worldPos, 1.0f), viewProjectionUnjittered);
    output.prevClipPos = mul(float4(worldPos, 1.0f), prevViewProjection);
    return output;
}
