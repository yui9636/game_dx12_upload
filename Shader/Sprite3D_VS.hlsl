// [Sprite3D_VS.hlsl]
#include "Sprite3D.hlsli"

VS_OUT main(VS_IN vin)
{
    VS_OUT vout;

    // 1. 座標変換
    // ローカル座標(モデル) -> ワールド座標(世界)
    // 入力はfloat3なので、w=1.0を付加して行列演算を行います
    float4 worldPos = mul(float4(vin.position, 1.0f), World);
    
    // ワールド -> ビュー(カメラ)
    float4 viewPos = mul(worldPos, View);
    
    // ビュー -> プロジェクション(画面)
    vout.position = mul(viewPos, Projection);

    // 2. パススルー
    // 色とUVはそのままピクセルシェーダーへ渡します
    vout.color = vin.color;
    vout.texcoord = vin.texcoord;

    return vout;
}