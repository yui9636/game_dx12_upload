// 頂点シェーダー出力データ
struct VS_OUT
{
	float4 position : SV_POSITION;
	float4 color    : COLOR;
    float2 texcoord : TEXCOORD;
};

cbuffer UIConstants : register(b0)
{
    float4 color; // 通常の乗算カラー (RGBA)
    float4 glowColor; // 発光色 (RGB)
    float glowIntensity; // 発光強度 (0.0 ~ )
};