#include "PrimitiveRenderer.hlsli"
// main はデバッグ用プリミティブの頂点変換または色出力を行う。

float4 main(VS_OUT pin) : SV_TARGET
{
	return pin.color;
}
