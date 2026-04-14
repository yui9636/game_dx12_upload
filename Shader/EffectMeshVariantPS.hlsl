#include "compute_particle.hlsli"

// ----------------------------------------------------------------
// テクスチャスロット (t2〜t8 は既存 t0/t1 の後ろに配置)
// ----------------------------------------------------------------
Texture2D gBaseTexture  : register(t2);   // ベース / メイン
Texture2D gMaskTexture  : register(t3);   // マスク / Dissolveノイズ
Texture2D gNormalMap    : register(t4);   // 法線マップ
Texture2D gFlowMap      : register(t5);   // フローマップ
Texture2D gSubTexture   : register(t6);   // サブテクスチャ
Texture2D gEmissionTex  : register(t7);   // エミッションマップ
SamplerState gSampler   : register(s1);

// ----------------------------------------------------------------
// エフェクト定数 (b3 — b0=CbScene, b2=RenderConstants を避ける)
// ----------------------------------------------------------------
cbuffer CbMeshEffect : register(b3)
{
    float  gDissolveAmount;     // 0..1
    float  gDissolveEdge;       // 発光幅
    float2 gFlowSpeed;          // UV流れ速度 (x,y)

    float4 gDissolveGlowColor;  // 縁発光色

    float  gFresnelPower;
    float3 _pad0;
    float4 gFresnelColor;

    float  gFlowStrength;
    float  gAlphaFade;          // ライフタイムα 0..1
    float2 gScrollSpeed;        // UVスクロール

    float  gDistortStrength;
    float3 _pad1;

    float4 gRimColor;
    float  gRimPower;
    float3 _pad2;

    float4 gEmissionColor;
    float  gEmissionIntensity;
    float  gEffectTime;         // EffectSystems から注入
    float2 _pad3;
};

// ----------------------------------------------------------------
struct PS_IN
{
    float4 position     : SV_POSITION;
    float2 texcoord     : TEXCOORD0;
    float4 color        : COLOR0;
    float3 worldNormal  : TEXCOORD1;
    float3 worldTangent : TEXCOORD2;
    float3 worldPos     : TEXCOORD3;
};

// ----------------------------------------------------------------
float4 main(PS_IN pin) : SV_TARGET0
{
    float2 uv = pin.texcoord;

    // --- UV Scroll ---
#ifdef USE_SCROLL
    uv += gScrollSpeed * gEffectTime;
#endif

    // --- Flow Map ---
#ifdef USE_FLOW_MAP
    float2 flow = gFlowMap.Sample(gSampler, uv).rg * 2.0f - 1.0f;
    uv += flow * gFlowStrength * gEffectTime;
#endif

    // --- Distort (サブテクスチャUV歪み) ---
#ifdef USE_DISTORT
    float2 distort = gSubTexture.Sample(gSampler, uv + gEffectTime * 0.05f).rg * 2.0f - 1.0f;
    uv += distort * gDistortStrength;
#endif

    // --- Base Color ---
    float4 color = pin.color;
#ifdef USE_TEXTURE
    color *= gBaseTexture.Sample(gSampler, uv);
#endif

    // --- Sub Texture (乗算合成) ---
#ifdef USE_SUB_TEXTURE
    float4 subCol = gSubTexture.Sample(gSampler, uv);
    color.rgb = lerp(color.rgb, color.rgb * subCol.rgb, subCol.a);
#endif

    // --- Normal Map & Lighting ---
#ifdef USE_NORMAL_MAP
    float3 N = normalize(pin.worldNormal);
    float3 T = normalize(pin.worldTangent);
    float3 B = cross(N, T);
    float3 nSample = gNormalMap.Sample(gSampler, uv).xyz * 2.0f - 1.0f;
    N = normalize(nSample.x * T + nSample.y * B + nSample.z * N);
    #ifdef USE_LIGHTING
    float3 L = normalize(-lightDirection.xyz);
    float diffuse = saturate(dot(N, L)) * 0.7f + 0.3f;
    color.rgb *= diffuse;
    #endif
#elif defined(USE_LIGHTING)
    float3 N = normalize(pin.worldNormal);
    float3 L = normalize(-lightDirection.xyz);
    float diffuse = saturate(dot(N, L)) * 0.7f + 0.3f;
    color.rgb *= diffuse;
#endif

    // --- Fresnel ---
#ifdef USE_FRESNEL
    float3 viewDir = normalize(cameraPosition.xyz - pin.worldPos);
    float3 Nf = normalize(pin.worldNormal);
    float fresnel = pow(1.0f - saturate(dot(Nf, viewDir)), gFresnelPower);
    color.rgb += gFresnelColor.rgb * fresnel * gFresnelColor.a;
    color.a   = saturate(color.a + gFresnelColor.a * fresnel);
#endif

    // --- Rim Light ---
#ifdef USE_RIM_LIGHT
    float3 viewDirR = normalize(cameraPosition.xyz - pin.worldPos);
    float rim = pow(1.0f - saturate(dot(normalize(pin.worldNormal), viewDirR)), gRimPower);
    color.rgb += gRimColor.rgb * rim * gRimColor.a;
#endif

    // --- Gradient Map ---
#ifdef USE_GRADIENT_MAP
    float lum = dot(color.rgb, float3(0.299f, 0.587f, 0.114f));
    color.rgb = gFlowMap.Sample(gSampler, float2(lum, 0.5f)).rgb; // gFlowMap をグラデマップ兼用
#endif

    // --- Mask ---
#ifdef USE_MASK
    float mask = gMaskTexture.Sample(gSampler, uv).r;
    color.a *= mask;
#endif

    // --- Dissolve ---
#ifdef USE_DISSOLVE
    float noise = gMaskTexture.Sample(gSampler, uv).r;
    float hardEdge = step(gDissolveAmount, noise);
    #ifdef USE_DISSOLVE_GLOW
    float glowMask = smoothstep(gDissolveAmount, gDissolveAmount + gDissolveEdge, noise)
                   * (1.0f - hardEdge);
    color.rgb = lerp(color.rgb, gDissolveGlowColor.rgb, glowMask * gDissolveGlowColor.a);
    #endif
    color.a *= hardEdge;
#endif

    // --- Chromatic Aberration ---
#ifdef USE_CHROMATIC_ABERRATION
    float2 offset = float2(0.005f, 0.0f) * gDistortStrength;
    float rr = gBaseTexture.Sample(gSampler, uv + offset).r;
    float bb = gBaseTexture.Sample(gSampler, uv - offset).b;
    color.r = lerp(color.r, rr, 0.6f);
    color.b = lerp(color.b, bb, 0.6f);
#endif

    // --- Emission ---
#ifdef USE_EMISSION
    float4 emCol = gEmissionTex.Sample(gSampler, uv);
    color.rgb += emCol.rgb * gEmissionColor.rgb * gEmissionIntensity;
#endif

    // --- Flipbook ---
#ifdef USE_FLIPBOOK
    // subUV はビルボードと同様にパーティクルから渡す想定 (将来拡張)
#endif

    // --- Toon ---
#ifdef USE_TOON
    float3 Nt = normalize(pin.worldNormal);
    float3 Lt = normalize(-lightDirection.xyz);
    float toonVal = dot(Nt, Lt) * 0.5f + 0.5f;
    toonVal = floor(toonVal * 3.0f) / 3.0f;
    color.rgb *= toonVal;
#endif

    // --- Side Fade ---
#ifdef USE_SIDE_FADE
    float sf = 1.0f - abs(pin.texcoord.x * 2.0f - 1.0f);
    color.a *= sf * sf;
#endif

    // --- Alpha Fade (ライフタイム) ---
#ifdef USE_ALPHA_FADE
    color.a *= gAlphaFade;
#endif

    color.a *= global_alpha;
    return color;
}
