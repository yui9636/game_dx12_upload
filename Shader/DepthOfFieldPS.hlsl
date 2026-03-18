#include "PostEffect.hlsli"

Texture2D<float4> SceneTexture : register(t0); // メイン画像
Texture2D<float4> BloomTexture : register(t1); // ブルーム画像
Texture2D<float> DepthTexture : register(t2); // 深度画像

SamplerState Sampler : register(s0);

struct PS_INPUT
{
    float4 position : SV_POSITION;
    float2 uv : TEXCOORD0;
};

float4 main(PS_INPUT input) : SV_TARGET
{
    // 1. 基本色とブルームの取得
    float4 color = SceneTexture.Sample(Sampler, input.uv);
    float4 bloom = BloomTexture.Sample(Sampler, input.uv);

    // 2. 深度の取得 (R成分のみ)
    float depthVal = DepthTexture.Sample(Sampler, input.uv).r;

    // 3. DoF (ボケ) の計算
    // 深度差に基づく簡易的なボケ強度計算
    // focusDistanceはメートル単位だが、ここでは深度(0.0-1.0)として簡易的に扱うか、
    // あるいは深度値をリニア変換して比較する必要がある。
    // 今回は「深度値の差」でボケを作る簡易実装とする。
    
    // 深度バッファが 0.0(手前) -> 1.0(奥) の場合
    // ピント位置を 0.01 倍するなどして調整
    float distDiff = abs(depthVal - (focusDistance * 0.001));
    
    float blurFactor = 0.0;
    if (distDiff > focusRange * 0.0001)
    {
        blurFactor = saturate((distDiff - focusRange * 0.0001) * bokehRadius);
    }

    // ボケ処理 (3x3 ボックスブラー)
    if (blurFactor > 0.01)
    {
        float4 blurredColor = 0;
        float totalWeight = 0;
        
        float2 texSize;
        SceneTexture.GetDimensions(texSize.x, texSize.y);
        float2 texelSize = 1.0 / texSize;
        
        // ループで周辺をサンプリング
        // blurFactorが大きいほど遠くをサンプリングしてボケを強くする
        for (int x = -1; x <= 1; x++)
        {
            for (int y = -1; y <= 1; y++)
            {
                float2 offset = float2(x, y) * texelSize * (blurFactor * 4.0);
                blurredColor += SceneTexture.Sample(Sampler, input.uv + offset);
                totalWeight += 1.0;
            }
        }
        color = blurredColor / totalWeight;
    }

    // 4. ブルーム合成 (加算)
    color += bloom * bloomIntensity;

    // 5. ヴィネット (周辺減光)
    float2 center = float2(0.5, 0.5);
    float dist = distance(input.uv, center);
    color.rgb *= (1.0 - smoothstep(0.4, 1.0, dist) * vignetteAmount);

    // 6. フラッシュ (ホワイトアウト)
    color.rgb += flashAmount;

    return color;
}