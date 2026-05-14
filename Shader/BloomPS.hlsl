// BloomPS.hlsl (モーションブラー統合版)
#include "FullScreenQuad.hlsli"
#include "PostEffect.hlsli"

Texture2D colorMap : register(t0);
Texture2D luminanceMap : register(t1);
// text 用色。ure2D depthMap : register(t2); // 深度は今回は未使用
Texture2D velocityMap : register(t3); // 速度バッファ

SamplerState linearSampler : register(s0);

// RGB から HSV への変換関数
float3 RGBtoHSV(float3 c)
{
    float4 K = float4(0.0, -1.0 / 3.0, 2.0 / 3.0, -1.0);
    float4 p = c.g < c.b ? float4(c.bg, K.wz) : float4(c.gb, K.xy);
    float4 q = c.r < p.x ? float4(p.xyw, c.r) : float4(c.r, p.yzx);
    float d = q.x - min(q.w, q.y);
    float e = 1e-10;
    return float3(abs((q.w - q.y) / (6.0 * d + e)), d / (q.x + e), q.x);
}

float3 HSVtoRGB(float3 c)
{
    float4 K = float4(1.0, 2.0 / 3.0, 1.0 / 3.0, 3.0);
    float3 p = abs(frac(c.xxx + K.xyz) * 6.0 - K.www);
    return c.z * lerp(K.xxx, saturate(p - K.xxx), c.y);
}

float3 ACESFilm(float3 x)
{
    float a = 2.51f;
    float b = 0.03f;
    float c = 2.43f;
    float d = 0.59f;
    float e = 0.14f;
    return saturate((x * (a * x + b)) / (x * (c * x + d) + e));
}

float4 main(VS_OUT pin) : SV_TARGET
{
    uint width, height;
    luminanceMap.GetDimensions(width, height);
    float2 texelSize = 1.0f / float2(width, height);

    // 1. シーンバッファから HDR の光データを取得
    float4 color = colorMap.Sample(linearSampler, pin.texcoord);
    float alpha = color.a;
// オブジェクト単位のモーションブラー（速度ベース）
float2 velocity = velocityMap.Sample(linearSampler, pin.texcoord).xy;
    
    // 速度がほぼ 0 の背景、またはブラー強度が 0 のときは処理を省く。
    if (motionBlurIntensity > 0.001f && length(velocity) > 0.0001f)
    {
        int numSamples = clamp((int) motionBlurSamples, 2, 32);
        float3 mbColor = 0.0f;
        
        // 中心から前後の軌跡へサンプリングし、残像を平均化する。
        [unroll(32)]
        for (int i = 0; i < numSamples; ++i)
        {
            float t = ((float) i / (float) (numSamples - 1)) - 0.5f;
            float2 sampleUV = pin.texcoord + velocity * t * motionBlurIntensity;
            mbColor += colorMap.SampleLevel(linearSampler, sampleUV, 0).rgb;
        }
        color.rgb = mbColor / (float) numSamples;
    }
// 川瀬式 16 タップブラー（ブルーム）
float3 blurColor = 0.0f;
    float spread = max(gaussianSigma, 1.5f);

    [unroll]
    for (int x = -1; x <= 2; ++x)
    {
        [unroll]
        for (int y = -1; y <= 2; ++y)
        {
            float2 offset = float2(x - 0.5f, y - 0.5f) * texelSize * spread;
            blurColor += luminanceMap.Sample(linearSampler, pin.texcoord + offset).rgb;
        }
    }
    blurColor /= 16.0f;

    // 2. ブルームを加算
    color.rgb += blurColor * bloomIntensity;
// 露出補正と ACES トーンマッピング
color.rgb *= exposure;
    color.rgb = ACESFilm(color.rgb);
// カラーグレーディング
float mono = dot(color.rgb, float3(0.299, 0.587, 0.114));
    color.rgb = lerp(color.rgb, mono.xxx, monoBlend);
    
    if (hueShift > 0.001f || hueShift < -0.001f)
    {
        float3 hsv = RGBtoHSV(color.rgb);
        hsv.x = frac(hsv.x + hueShift);
        color.rgb = HSVtoRGB(hsv);
    }
    
    float2 uvToCenter = abs(pin.texcoord - float2(0.5f, 0.5f));
    float vignette = 1.0f - saturate(length(uvToCenter) * 2.0f);
    color.rgb *= lerp(1.0f, vignette, vignetteAmount);

    color.rgb = lerp(color.rgb, float3(1.0f, 1.0f, 1.0f), flashAmount);
// 最終仕上げのガンマ補正
const float INV_GAMMA = 1.0f / 2.2f;
    color.rgb = pow(color.rgb, INV_GAMMA);

    return float4(color.rgb, alpha);
}
