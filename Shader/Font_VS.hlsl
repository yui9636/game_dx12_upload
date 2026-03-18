
#include "Font.hlsli"

VS_OUT main(
	float4 position     : POSITION,
	float4 color        : COLOR,
	float2 texcoord     : TEXCOORD
	)
{
	VS_OUT vout;
	
	// ★修正: Sprite3D_VS.hlsl と全く同じ計算手順にする
    // 1. ローカル -> ワールド
    // ※入力positionはfloat4ですが、w=1.0として扱います
    float4 worldPos = mul(position, World);

    // 2. ワールド -> ビュー
    float4 viewPos = mul(worldPos, View);

    // 3. ビュー -> プロジェクション
    vout.position = mul(viewPos, Projection);

    vout.color = color;
    vout.texcoord = texcoord;

    return vout;

}
