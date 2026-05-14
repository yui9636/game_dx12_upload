#include "PrimitiveRenderer.hlsli"
// main はデバッグ用プリミティブの頂点変換または色出力を行う。

VS_OUT main(VS_IN vin)
{
	VS_OUT vout;
	vout.position = mul(vin.position, viewProjection);
	vout.color = vin.color;

	return vout;
}
