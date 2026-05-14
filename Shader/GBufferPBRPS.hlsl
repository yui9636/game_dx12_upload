#include "PBR.hlsli"

Texture2D AlbedoMap : register(t0);
Texture2D NormalMap : register(t1);
Texture2D MRMap : register(t2);
SamplerState LinearSamp : register(s0);

static const float GAMMA = 2.2f;

struct PS_OUTPUT
{
    float4 albedoMetallic : SV_TARGET0;
    float4 normalRoughness : SV_TARGET1;
    float4 worldPosDepth : SV_TARGET2;
    float2 velocity : SV_TARGET3;
};

PS_OUTPUT main(VS_OUT pin)
{
    PS_OUTPUT output;
    // マテリアルテクスチャと法線マップから GBuffer 出力値を組み立てる。
    float3 albedoLin = pow(AlbedoMap.Sample(LinearSamp, pin.texcoord).rgb, GAMMA);
    albedoLin *= materialColor.rgb;
    
    float2 rm = MRMap.Sample(LinearSamp, pin.texcoord).gb;
    float roughness = clamp(rm.x * roughnessFactor, 0.05f, 1.0f);
    float metallic = saturate(rm.y * metallicFactor);

    output.albedoMetallic = float4(albedoLin, metallic);

    float3 N = normalize(pin.normal);
    float3 T = normalize(pin.tangent);
    T = normalize(T - N * dot(N, T));
    float3 B = normalize(cross(N, T));
    float3 nMap = NormalMap.Sample(LinearSamp, pin.texcoord).xyz * 2.0f - 1.0f;
    N = normalize(nMap.x * T + nMap.y * B + nMap.z * N);

    output.normalRoughness = float4(N, roughness);

    // WorldPos と Depth を GBuffer へ書き込む。
    output.worldPosDepth = float4(pin.position, pin.vertex.z);

    float4 stableCurClip = mul(float4(pin.position, 1.0f), viewProjectionUnjittered);
    float2 currentNDC = stableCurClip.xy / stableCurClip.w;

    // 直前フレームの座標は VS で計算済みの prevClipPos を使う。
    // prevViewProjection はジッターなしなので、ここでは再計算しない。
    float2 prevNDC = pin.prevClipPos.xy / pin.prevClipPos.w;

    float2 currentUV = currentNDC * float2(0.5f, -0.5f) + 0.5f;
    float2 prevUV = prevNDC * float2(0.5f, -0.5f) + 0.5f;
    
    // FSR2 の仕様に合わせ、「過去 - 現在」を出力する。
    output.velocity = prevUV - currentUV;
    
   
    return output;
}
