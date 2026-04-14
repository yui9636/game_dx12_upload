cbuffer CbTrail : register(b0)
{
    float4x4 gViewProjection;
};

struct VS_IN
{
    float3 position : POSITION;
    float4 color    : COLOR;
    float2 texcoord : TEXCOORD;
};

struct VS_OUT
{
    float4 position : SV_POSITION;
    float4 color    : COLOR;
    float2 texcoord : TEXCOORD;
};

VS_OUT main(VS_IN vin)
{
    VS_OUT vout;
    vout.position = mul(float4(vin.position, 1.0f), gViewProjection);
    vout.color = vin.color;
    vout.texcoord = vin.texcoord;
    return vout;
}
