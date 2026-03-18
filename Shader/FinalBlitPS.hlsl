// FinalBlitPS.hlsl
// SceneColor (HDR) をトーンマップしてバックバッファ (LDR) に出力する
// DX12パス専用（DX11はFSR2/PostEffectが担当）

Texture2D SceneColor : register(t0);
SamplerState LinearSampler : register(s0);

struct VS_OUT
{
    float4 position : SV_POSITION;
    float2 texcoord : TEXCOORD0;
};

// ACES フィルミックトーンマップ
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
    float3 hdr = SceneColor.Sample(LinearSampler, pin.texcoord).rgb;

    // ACES トーンマップ
    float3 ldr = ACESFilm(hdr);

    // リニア -> sRGB ガンマ補正
    ldr = pow(max(ldr, 0.0001f), 1.0f / 2.2f);

    return float4(ldr, 1.0f);
}
