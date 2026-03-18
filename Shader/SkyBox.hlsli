#include "ShadingFunctions.hlsli"

cbuffer CbScene : register(b0)
{
    row_major float4x4 inverseViewProjection;
};

struct VS_OUT
{
    float4 svPosition : SV_POSITION; 
    float3 rayDir : TEXCOORD0; 
};