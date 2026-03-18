struct VS_IN
{
    float4 pos : POSITION;
    float4 uv : TEXCOORD0;
};
struct VS_OUT
{
    float4 pos : SV_POSITION;
    float4 uv : TEXCOORD0;
    float vCoord : TEXCOORD1; 
};



cbuffer CbScene : register(b0)
{
    row_major float4x4 viewProjection;
    float headOffset; 
    float3 _dummy; 
};

