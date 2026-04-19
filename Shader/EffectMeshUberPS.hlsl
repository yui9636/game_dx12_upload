// EffectMeshUberPS — Phase B Route B
// 単一 PS で variant flag のランタイム分岐。Route A (変種コンパイル) に戻す場合は
// MeshFlag_* の if ブロックを #ifdef USE_* に戻すだけで済む。

Texture2D gBaseTexture  : register(t0);
Texture2D gMaskTexture  : register(t1);
Texture2D gNormalMap    : register(t2);
Texture2D gFlowMap      : register(t3);
Texture2D gSubTexture   : register(t4);
Texture2D gEmissionTex  : register(t5);
SamplerState gSampler   : register(s0);

// EffectMeshShaderFlags (EffectMeshVariant.h と同値)
#define MeshFlag_Texture             (1u << 0)
#define MeshFlag_Dissolve            (1u << 1)
#define MeshFlag_Distort             (1u << 2)
#define MeshFlag_Lighting            (1u << 3)
#define MeshFlag_Mask                (1u << 4)
#define MeshFlag_Fresnel             (1u << 5)
#define MeshFlag_Flipbook            (1u << 6)
#define MeshFlag_GradientMap         (1u << 7)
#define MeshFlag_ChromaticAberration (1u << 8)
#define MeshFlag_DissolveGlow        (1u << 9)
#define MeshFlag_MatCap              (1u << 10)
#define MeshFlag_NormalMap           (1u << 11)
#define MeshFlag_FlowMap             (1u << 12)
#define MeshFlag_SideFade            (1u << 13)
#define MeshFlag_AlphaFade           (1u << 14)
#define MeshFlag_SubTexture          (1u << 15)
#define MeshFlag_Toon                (1u << 16)
#define MeshFlag_RimLight            (1u << 17)
#define MeshFlag_VertexColorBlend    (1u << 18)
#define MeshFlag_Emission            (1u << 19)
#define MeshFlag_Scroll              (1u << 20)

cbuffer CbMeshEffect : register(b3)
{
    float  gDissolveAmount;
    float  gDissolveEdge;
    float2 gFlowSpeed;

    float4 gDissolveGlowColor;

    float  gFresnelPower;
    float3 _pad0;
    float4 gFresnelColor;

    float  gFlowStrength;
    float  gAlphaFade;
    float2 gScrollSpeed;

    float  gDistortStrength;
    float3 _pad1;

    float4 gRimColor;
    float  gRimPower;
    float3 _pad2;

    float4 gEmissionColor;
    float  gEmissionIntensity;
    float  gEffectTime;
    float2 _pad3;

    float4 gBaseColor;      // packet baseColor × material color
    uint   gVariantFlags;
    float3 _pad4;
    float4 gLightDirection; // VS の lightDirection を複製 (PS でも使う場合)
    float4 gCameraPosition;
};

struct PS_IN
{
    float4 position     : SV_POSITION;
    float2 texcoord     : TEXCOORD0;
    float4 color        : COLOR0;
    float3 worldNormal  : TEXCOORD1;
    float3 worldTangent : TEXCOORD2;
    float3 worldPos     : TEXCOORD3;
};

float4 main(PS_IN pin) : SV_TARGET0
{
    float2 uv = pin.texcoord;

    if (gVariantFlags & MeshFlag_Scroll) {
        uv += gScrollSpeed * gEffectTime;
    }

    if (gVariantFlags & MeshFlag_FlowMap) {
        float2 flow = gFlowMap.Sample(gSampler, uv).rg * 2.0f - 1.0f;
        uv += flow * gFlowStrength * gEffectTime;
    }

    if (gVariantFlags & MeshFlag_Distort) {
        float2 distort = gSubTexture.Sample(gSampler, uv + gEffectTime * 0.05f).rg * 2.0f - 1.0f;
        uv += distort * gDistortStrength;
    }

    float4 color = pin.color * gBaseColor;
    if (gVariantFlags & MeshFlag_Texture) {
        color *= gBaseTexture.Sample(gSampler, uv);
    }

    if (gVariantFlags & MeshFlag_SubTexture) {
        float4 subCol = gSubTexture.Sample(gSampler, uv);
        color.rgb = lerp(color.rgb, color.rgb * subCol.rgb, subCol.a);
    }

    float3 N = normalize(pin.worldNormal);
    if (gVariantFlags & MeshFlag_NormalMap) {
        float3 T = normalize(pin.worldTangent);
        float3 B = cross(N, T);
        float3 nSample = gNormalMap.Sample(gSampler, uv).xyz * 2.0f - 1.0f;
        N = normalize(nSample.x * T + nSample.y * B + nSample.z * N);
    }
    if (gVariantFlags & MeshFlag_Lighting) {
        float3 L = normalize(-gLightDirection.xyz);
        float diffuse = saturate(dot(N, L)) * 0.7f + 0.3f;
        color.rgb *= diffuse;
    }

    if (gVariantFlags & MeshFlag_Fresnel) {
        float3 viewDir = normalize(gCameraPosition.xyz - pin.worldPos);
        float fresnel = pow(1.0f - saturate(dot(N, viewDir)), gFresnelPower);
        color.rgb += gFresnelColor.rgb * fresnel * gFresnelColor.a;
        color.a   = saturate(color.a + gFresnelColor.a * fresnel);
    }

    if (gVariantFlags & MeshFlag_RimLight) {
        float3 viewDir = normalize(gCameraPosition.xyz - pin.worldPos);
        float rim = pow(1.0f - saturate(dot(N, viewDir)), gRimPower);
        color.rgb += gRimColor.rgb * rim * gRimColor.a;
    }

    if (gVariantFlags & MeshFlag_GradientMap) {
        float lum = dot(color.rgb, float3(0.299f, 0.587f, 0.114f));
        color.rgb = gFlowMap.Sample(gSampler, float2(lum, 0.5f)).rgb;
    }

    if (gVariantFlags & MeshFlag_Mask) {
        float mask = gMaskTexture.Sample(gSampler, uv).r;
        color.a *= mask;
    }

    if (gVariantFlags & MeshFlag_Dissolve) {
        float noise = gMaskTexture.Sample(gSampler, uv).r;
        float hardEdge = step(gDissolveAmount, noise);
        if (gVariantFlags & MeshFlag_DissolveGlow) {
            float glowMask = smoothstep(gDissolveAmount, gDissolveAmount + gDissolveEdge, noise)
                           * (1.0f - hardEdge);
            color.rgb = lerp(color.rgb, gDissolveGlowColor.rgb, glowMask * gDissolveGlowColor.a);
        }
        color.a *= hardEdge;
    }

    if (gVariantFlags & MeshFlag_ChromaticAberration) {
        float2 offset = float2(0.005f, 0.0f) * gDistortStrength;
        float rr = gBaseTexture.Sample(gSampler, uv + offset).r;
        float bb = gBaseTexture.Sample(gSampler, uv - offset).b;
        color.r = lerp(color.r, rr, 0.6f);
        color.b = lerp(color.b, bb, 0.6f);
    }

    if (gVariantFlags & MeshFlag_Emission) {
        float4 emCol = gEmissionTex.Sample(gSampler, uv);
        color.rgb += emCol.rgb * gEmissionColor.rgb * gEmissionIntensity;
    }

    if (gVariantFlags & MeshFlag_Toon) {
        float3 Lt = normalize(-gLightDirection.xyz);
        float toonVal = dot(N, Lt) * 0.5f + 0.5f;
        toonVal = floor(toonVal * 3.0f) / 3.0f;
        color.rgb *= toonVal;
    }

    if (gVariantFlags & MeshFlag_SideFade) {
        float sf = 1.0f - abs(pin.texcoord.x * 2.0f - 1.0f);
        color.a *= sf * sf;
    }

    if (gVariantFlags & MeshFlag_AlphaFade) {
        color.a *= gAlphaFade;
    }

    return color;
}
