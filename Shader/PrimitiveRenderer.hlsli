// PrimitiveRenderer はこの描画経路で共有する入力構造や定数を定義する。
struct VS_IN
{
	float4 position : POSITION;
	float4 color : COLOR;
};

struct VS_OUT
{
	float4 position : SV_POSITION;
	float4 color : COLOR;
};

cbuffer CbScene : register(b0)
{
	row_major float4x4	viewProjection;
};
