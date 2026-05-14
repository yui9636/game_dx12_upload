#include "ShadingFunctions.hlsli"
// SkyBox はこの描画経路で共有する入力構造や定数を定義する。

cbuffer CbScene : register(b0)
{
    row_major float4x4 inverseViewProjection;
};

struct VS_OUT
{
    float4 svPosition : SV_POSITION; 
    float3 rayDir : TEXCOORD0; 
};