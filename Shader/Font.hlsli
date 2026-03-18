struct VS_OUT
{
	float4 position : SV_POSITION;
	float2 texcoord : TEXCOORD;
	float4 color    : COLOR;
};

// 定数バッファ (C++側で調整可能にするためのパラメータ)
cbuffer SDFParams : register(b0)
{
    float4 Color; // 文字色 (頂点カラーと乗算)
    float Threshold; // 輪郭の閾値 (基本は0.5)
    float Softness; // 輪郭の柔らかさ (0に近いほどシャープ)
    float2 Padding;
}

cbuffer CBMatrix : register(b1)
{
    matrix World;
    matrix View;
    matrix Projection;
}