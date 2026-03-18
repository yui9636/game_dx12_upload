#include "PBR.hlsli"

Texture2D AlbedoMap : register(t0);
Texture2D NormalMap : register(t1);
Texture2D MetallicMap : register(t2);
Texture2D RoughnessMap : register(t3);
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

    // --- [ 1. }eAE@̌vZ ] ---
    float3 albedoSrgb = saturate(AlbedoMap.Sample(LinearSamp, pin.texcoord).rgb);
    if (dot(albedoSrgb, float3(0.2126f, 0.7152f, 0.0722f)) < 1e-4f)
    {
        albedoSrgb = max(materialColor.rgb, float3(0.7f, 0.7f, 0.7f));
    }

    float3 albedoLin = pow(albedoSrgb, GAMMA);
    albedoLin *= max(materialColor.rgb, float3(0.35f, 0.35f, 0.35f));
    
    float metallicSample = MetallicMap.Sample(LinearSamp, pin.texcoord).r;
    float roughnessSample = RoughnessMap.Sample(LinearSamp, pin.texcoord).r;
    float roughness = clamp(roughnessSample * roughnessFactor, 0.05f, 1.0f);
    float metallic = saturate(metallicSample * metallicFactor);

    output.albedoMetallic = float4(albedoLin, metallic);

    float3 N = normalize(pin.normal);
    float3 T = normalize(pin.tangent);
    T = normalize(T - N * dot(N, T));
    float3 B = normalize(cross(N, T));
    float3 nMap = NormalMap.Sample(LinearSamp, pin.texcoord).xyz;
    if (length(nMap) < 1e-4f)
    {
        nMap = float3(0.5f, 0.5f, 1.0f);
    }
    nMap = nMap * 2.0f - 1.0f;
    N = normalize(nMap.x * T + nMap.y * B + nMap.z * N);

    output.normalRoughness = float4(N, roughness);

    // --- [ 2. [x (Depth) ] ---
    // SV_Position.z ͂łɗhĂ̂ŁAKvɉ pin.curClipPos.z / pin.curClipPos.w ɕς
    // [xx[X̃|XgGtFNg肵܂B܂͌ێ pin.vertex.z OKłB
    output.worldPosDepth = float4(pin.position, pin.vertex.z);

    float4 stableCurClip = mul(float4(pin.position, 1.0f), viewProjectionUnjittered);
    float2 currentNDC = stableCurClip.xy / stableCurClip.w;

    // ߋ̍W VSŌvZꂽ[J̓iprevWorldMatrixj𔽉f̂̂܂܎g
    // iprevViewProjection ͌XWb^[Ȃ̂ňZ͐΂ɍsȂj
    float2 prevNDC = pin.prevClipPos.xy / pin.prevClipPos.w;

    float2 currentUV = currentNDC * float2(0.5f, -0.5f) + 0.5f;
    float2 prevUV = prevNDC * float2(0.5f, -0.5f) + 0.5f;
    
    // FSR2̎dlɏ]uߋ - ݁vóiŊSȃWb^[t[ƂȂj
    output.velocity = prevUV - currentUV;
    
   
    return output;
}