#include "sprite.hlsli"

// テクスチャとサンプラー
Texture2D tex : register(t0);
SamplerState sam : register(s0);

float4 main(VS_OUT pin) : SV_TARGET
{
   // 1. テクスチャの色をサンプリング
    float4 texColor = tex.Sample(sam, pin.texcoord);

    // 2. 基本カラーの計算 (乗算合成)
    // テクスチャ色 × 頂点カラー × コンポーネント設定色
    float4 finalColor = texColor * pin.color * color;

    // 3. アルファテスト
    if (finalColor.a <= 0.0)
    {
        discard;
    }

    // 4. 発光エフェクト (加算合成)
    // 発光色(rgb) × 強度(a) × テクスチャのアルファ(a)
    // ※ texColor.a を掛けることで、絵の透明な部分は光らないようにする
    float3 glow = glowColor.rgb * glowColor.a * texColor.a;

    // 最終カラーに発光分を足し込む (RGBのみ)
    finalColor.rgb += glow;

    return finalColor;
    
 
}