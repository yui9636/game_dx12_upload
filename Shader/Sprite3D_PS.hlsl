// [SpriteUI_PS.hlsl]
#include "Sprite3D.hlsli"

// テクスチャリソース
Texture2D g_Texture : register(t0);
SamplerState g_Sampler : register(s0);

float4 main(VS_OUT pin) : SV_TARGET
{
    float2 uv = pin.texcoord;

    // -----------------------------------------------------------
    // 1. HPプログレスのカット処理
    // -----------------------------------------------------------
    // UVの横軸(x)が Progress を超えていたら描画を破棄(discard)します。
    // 板自体が3D空間で回転しているため、この垂直カットだけで
    // 視覚的には「パースの効いた斜めの断面」として正しく見えます。
    if (uv.x > Progress)
    {
        discard;
    }

    // -----------------------------------------------------------
    // 2. 画像サンプリング
    // -----------------------------------------------------------
    float4 texColor = g_Texture.Sample(g_Sampler, uv);

    // -----------------------------------------------------------
    // 3. エッジのアンチエイリアス (自動AA)
    // -----------------------------------------------------------
    // 3D空間で板を鋭角に傾けると、テクスチャの縁がギザギザしやすくなります。
    // fwidth関数を使い、画面上のピクセル密度に応じてボケ足を自動計算し、
    // UVの境界(0.0と1.0)付近を滑らかにフェードさせます。
    
    float2 aaWidth = fwidth(uv) * 1.5; // ボケ幅の強度調整
    
    // smoothstepで境界付近のアルファを0~1に滑らかにする
    float2 edgeAlpha = smoothstep(0.0, aaWidth, uv) * (1.0 - smoothstep(1.0 - aaWidth, 1.0, uv));
    
    // 上下左右の最小値をとって枠全体のアルファとする
    float borderFactor = edgeAlpha.x * edgeAlpha.y;

    // -----------------------------------------------------------
    // 4. 最終出力
    // -----------------------------------------------------------
    // テクスチャ色 * 発光カラー(C++) * 頂点カラー
    float3 finalRGB = texColor.rgb * ColorCore.rgb * pin.color.rgb;
    float finalA = texColor.a * ColorCore.a * pin.color.a * borderFactor;

    // 透明度チェック (ゴミピクセル除去)
    if (finalA < 0.01)
        discard;

    return float4(finalRGB, finalA);
}