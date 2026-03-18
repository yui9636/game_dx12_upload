
#include "Font.hlsli"

Texture2D diffuseMap : register(t0);
SamplerState diffuseMapSamplerState : register(s0);

float4 main(VS_OUT pin) : SV_TARGET
{
    // 1. SDFテクスチャから距離情報をサンプリング
    // 一般的なSDFフォントはAlphaやRedチャンネルに距離が格納されています
    float dist = diffuseMap.Sample(diffuseMapSamplerState, pin.texcoord).a;

    // 2. 輪郭の抽出 (SDFロジック)
    // fwidthを使うことで、画面上のピクセル密度に合わせて滑らかさを自動調整します
    // これにより、拡大してもボケず、縮小してもジャギーが出にくくなります
    float smoothing = fwidth(dist) * Softness;
    float alpha = smoothstep(Threshold - smoothing, Threshold + smoothing, dist);

    // 3. 文字色と頂点カラーの合成
    // SDFの結果(alpha)を不透明度として適用
    float4 finalColor = pin.color * Color;
    finalColor.a *= alpha;

    // アルファテスト (完全に透明なピクセルは描画しない)
    clip(finalColor.a - 0.01f);

    return finalColor;
}