#include"Effect.hlsl"
#include "Skinning.hlsli"

VS_OUT main(
float4 position : POSITION,
float4 boneWeights : BONE_WEIGHTS,
uint4 boneIndices : BONE_INDICES,
float2 texcoord : TEXCOORD,
float3 normal : NORMAL,
float3 tangent : TANGENT,
float4 color : COLOR
)
{
    VS_OUT vout = (VS_OUT) 0;
    
    //position = SkinningPosition(position, boneWeights, boneIndices);
    //vout.vertex = mul(position, viewProjection);
    //vout.texcoord = texcoord;
    //vout.normal = SkinningVector(normal, boneWeights, boneIndices);
    //vout.position = position.xyz;
    //vout.tangent = SkinningVector(tangent, boneWeights, boneIndices);
    
    //float4 shadow = mul(position, lightViewProjection);
    //shadow.xyz /= shadow.w;
    //shadow.y = -shadow.y;
    //shadow.xy = shadow.xy * 0.5f + 0.5f;
    //vout.shadow = shadow.xyz;
   
    //return vout;
    
    // 1. まずワールド座標へ変換 (スキニング)
    // この時点で position はモデルの変形後の「ワールド座標」になります
    position = SkinningPosition(position, boneWeights, boneIndices);

    // 2. ★重要: 法線も先にワールド空間へ変換する
    // WPOで「法線の向き」に動かすため、先に計算しておく必要があります
    float3 worldNormal = SkinningVector(normal, boneWeights, boneIndices);

    // -----------------------------------------------------
    // ★追加: WPO (World Position Offset) 処理
    // -----------------------------------------------------
    // 強度が設定されている場合のみ計算 (if分岐)
    if (abs(wpoStrength) > 0.001f)
    {
        // 位相 (時間 x 速さ)
        float phase = currentTime * wpoSpeed;

        // サイン波の計算
        // position.y (高さ) に応じて波のタイミングをずらすことで「うねり」を作ります
        float wave = sin(phase + position.y * wpoFrequency);

        // ワールド座標の法線方向に、波の強さを乗算して加算
        position.xyz += worldNormal * wave * wpoStrength;
    }
    // -----------------------------------------------------

    // 3. ビュー・プロジェクション変換 (ワールド -> スクリーン)
    // WPOでずらした position を使って変換します
    vout.vertex = mul(position, viewProjection);
    
    vout.texcoord = texcoord;
    
    // 計算済みのワールド法線を代入
    vout.normal = worldNormal;
    
    // PSに渡す座標も、ずらした後の position を渡す
    vout.position = position.xyz;
    
    vout.tangent = SkinningVector(tangent, boneWeights, boneIndices);
    
    vout.color = color;
    
    // シャドウ計算 (ここもWPO後の座標で行う方が影が合致します)
    float4 shadow = mul(position, lightViewProjection);
    shadow.xyz /= shadow.w;
    shadow.y = -shadow.y;
    shadow.xy = shadow.xy * 0.5f + 0.5f;
    vout.shadow = shadow.xyz;
    
    return vout;
    

}
