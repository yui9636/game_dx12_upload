#include "SwordTrail.hlsli"

Texture2D BaseTex : register(t0); // コア強度
Texture2D MaskTex : register(t1); // マスク
Texture2D NoiseTex : register(t2); // ノイズ
SamplerState LinearClamp : register(s0);

float4 main(VS_OUT pin) : SV_TARGET
{
    float2 uv = pin.uv;
    uv.x = clamp(uv.x, 0.004, 0.996);

    // マスク
    float mask = MaskTex.Sample(LinearClamp, uv).r;
    if (mask < 0.01)
        discard;

    // アルファの基本
    float a = smoothstep(0.0, 0.02, mask);

    // テイル側（U=1）をフェード
    float tailSideFade = 1.0 - smoothstep(0.9, 1.0, uv.x);
    a *= tailSideFade;

    // 中央縦線を抑える
    float centerSoft = smoothstep(0.45, 0.55, abs(uv.x - 0.5));
    a *= 1.0 - centerSoft * 0.5;

    // 先端・末端のフェード
    // vCoord が必要 → uv.y を流用
    float headFade = smoothstep(0.0, 0.1, uv.y); // 0?0.1でフェードイン
    float tailFade = 1.0 - smoothstep(0.9, 1.0, uv.y); // 0.9?1.0でフェードアウト
    a *= headFade * tailFade;

    // コア
    float coreTex = BaseTex.Sample(LinearClamp, uv).r;

    // ノイズ
    float noiseVal = NoiseTex.Sample(LinearClamp, uv).r;
    float flicker = lerp(0.8, 1.2, noiseVal);

    // 中心ブースト
    float center = saturate(1.0 - abs(uv.x * 2.0 - 1.0));
    center = pow(center, 3.0);

    float lum = mask * flicker * (0.5 + 0.5 * center) * (1.0 + coreTex);

    // 真紅のグラデ
    const float3 colCore = float3(1.8, 0.1, 0.1); // より赤い
    const float3 colEdge = float3(0.15, 0.00, 0.00);

    float3 color = lerp(colEdge, colCore, center);
    color *= lum * 4.0; // 輝度調整

    return float4(color, a);
}
