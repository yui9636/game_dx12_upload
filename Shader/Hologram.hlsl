//=============================================================================
// Hologram.hlsli
// ホログラム残像用の共通定義
//=============================================================================

// 頂点シェーダーからの出力構造体
struct VS_OUT
{
    float4 position : SV_POSITION; // スクリーン座標
    float3 worldPos : POSITION; // ワールド座標
    float3 normal : NORMAL; // 法線
    float2 texcoord : TEXCOORD0; // UV座標（今回は計算メインだが念のため）
};


cbuffer CbScene : register(b0)
{
    row_major float4x4 viewProjection;
    float4 lightDirection;
    float4 lightColor;
    float4 cameraPosition;
    row_major float4x4 lightViewProjection;
};

cbuffer CbHologram : register(b1)
{
    float4 baseColor; // ベース色 (RGB) + 全体アルファ (A)
    float4 rimColor; // リム発光色
    
    float fresnelPower; // 4 bytes
    float scanlineFreq; // 4 bytes
    float scanlineSpeed; // 4 bytes
    float glitchIntensity; // 4 bytes (計 16 bytes)

    
    float time; // 4 bytes (C++と一致)
    float alpha; // ★修正: C++に合わせて float alpha を明示
    float2 _padding; // 8 bytes (計 16 bytes)
};