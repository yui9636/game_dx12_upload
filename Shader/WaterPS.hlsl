cbuffer WaterCB : register(b0)
{
    float4x4 viewProj;
    float4 worldOffsetSeaLevel;
    float4 shallowColor;
    float4 deepColor;
    float4 params;
    float4 cameraPosition;
    float4 screenParams;
    float4 waterFeatureFlags;
};

Texture2D gGBuffer2  : register(t0);
Texture2D gPrevScene : register(t1);
SamplerState gPointClamp  : register(s2);
SamplerState gLinearClamp : register(s3);

struct PS_INPUT
{
    float4 position   : SV_Position;
    float3 worldPos   : TEXCOORD0;
    float2 uv         : TEXCOORD1;
    float  shore      : TEXCOORD2;
    float3 baseNormal : TEXCOORD3;
    float  crest      : TEXCOORD4;
};

float Hash21(float2 p)
{
    p = frac(p * float2(123.34f, 456.21f));
    p += dot(p, p + 45.32f);
    return frac(p.x * p.y);
}

float ValueNoise(float2 p)
{
    float2 i = floor(p);
    float2 f = frac(p);
    float a = Hash21(i);
    float b = Hash21(i + float2(1.0f, 0.0f));
    float c = Hash21(i + float2(0.0f, 1.0f));
    float d = Hash21(i + float2(1.0f, 1.0f));
    float2 u = f * f * (3.0f - 2.0f * f);
    return lerp(lerp(a, b, u.x), lerp(c, d, u.x), u.y);
}

float Fbm3(float2 p)
{
    float v = 0.0f;
    float a = 0.5f;
    [unroll]
    for (int i = 0; i < 3; ++i) {
        v += ValueNoise(p) * a;
        p *= 2.04f;
        a *= 0.54f;
    }
    return v;
}

float3 RippleNormal(float2 worldXZ, float time, float waveSpeed)
{
    float2 p1 = worldXZ * 0.24f + float2(time * 0.12f, time * 0.07f) * waveSpeed;
    float2 p2 = worldXZ * 0.55f + float2(-time * 0.10f, time * 0.16f) * waveSpeed;
    float e = 0.35f;
    float dx = (Fbm3(p1 + float2(e, 0.0f)) - Fbm3(p1 - float2(e, 0.0f))) * 0.75f;
    dx += (Fbm3(p2 + float2(e, 0.0f)) - Fbm3(p2 - float2(e, 0.0f))) * 0.35f;
    float dz = (Fbm3(p1 + float2(0.0f, e)) - Fbm3(p1 - float2(0.0f, e))) * 0.75f;
    dz += (Fbm3(p2 + float2(0.0f, e)) - Fbm3(p2 - float2(0.0f, e))) * 0.35f;
    return normalize(float3(-dx, 1.0f, -dz));
}

float SchlickFresnel(float cosTheta, float f0)
{
    float m = 1.0f - saturate(cosTheta);
    float m2 = m * m;
    return f0 + (1.0f - f0) * m2 * m2 * m;
}

float3 ProceduralSky(float3 reflDir, float3 lightDir)
{
    float up = saturate(reflDir.y * 0.5f + 0.5f);
    float3 horizon = float3(0.58f, 0.72f, 0.82f);
    float3 zenith = float3(0.16f, 0.34f, 0.62f);
    float3 sky = lerp(horizon, zenith, smoothstep(0.35f, 1.0f, up));
    float sun = pow(saturate(dot(reflDir, lightDir)), 220.0f);
    return sky + float3(2.3f, 2.0f, 1.55f) * sun;
}

// Organic caustic pattern: Voronoi-flavoured cells with fine-grain detail.
// Replaces the previous sin*sin pattern which produced clearly visible
// arc-shaped interference bands across the water surface.
float Caustics(float2 worldXZ, float time)
{
    float2 p = worldXZ * 0.55f;
    float2 i = floor(p);
    float2 f = frac(p);

    // Cheap 3x3 Voronoi (F1 distance) for irregular spots.
    float minDist = 1.0f;
    [unroll]
    for (int dy = -1; dy <= 1; ++dy) {
        [unroll]
        for (int dx = -1; dx <= 1; ++dx) {
            float2 g = float2((float)dx, (float)dy);
            float2 seed = i + g;
            float h = Hash21(seed);
            // Animate each cell point in a small circle.
            float angle = h * 6.2831853f + time * (0.6f + h * 0.7f);
            float2 cellPoint = g + 0.5f + 0.45f * float2(cos(angle), sin(angle));
            float2 diff = cellPoint - f;
            float d = dot(diff, diff);
            minDist = min(minDist, d);
        }
    }
    // Highlights at cell borders.
    float caustic = smoothstep(0.30f, 0.05f, minDist);
    // Add a soft secondary band to break the regularity.
    float band = Fbm3(worldXZ * 0.22f + time * float2(0.04f, -0.03f));
    return saturate(caustic * (0.45f + band * 0.55f));
}

float4 main(PS_INPUT input) : SV_Target
{
    // Mesh-level culling now removes dry-land quads; just trim near-shore
    // pixels with no actual depth.
    if (input.shore < 0.015f) discard;

    float time = params.x;
    float waveSpeed = params.y;
    float depthFade = max(params.w, 0.001f);

    float3 ripple = RippleNormal(input.worldPos.xz, time, waveSpeed);
    float3 normal = normalize(input.baseNormal + ripple * 0.18f);
    float3 viewDir = normalize(cameraPosition.xyz - input.worldPos);
    float3 lightDir = normalize(float3(-0.35f, 0.82f, -0.28f));

    float distView = length(cameraPosition.xyz - input.worldPos);
    float farFade = saturate(1.0f - distView * 0.0020f);
    normal = normalize(lerp(float3(0.0f, 1.0f, 0.0f), normal, farFade));

    float2 screenUV = input.position.xy * screenParams.zw;
    float shallowMask = 1.0f - smoothstep(0.18f, 0.88f, input.shore);
    float shoreSafe = smoothstep(0.10f, 0.42f, input.shore);
    float2 refractOffset = normal.xz * lerp(0.002f, 0.018f, shoreSafe) * farFade;
    float2 refractUV = saturate(screenUV + refractOffset);

    // Two GBuffer2 samples: straight (this exact screen pixel) for the
    // discard / depth comparison, refract-offset for visual refraction.
    float4 g2Straight = gGBuffer2.SampleLevel(gPointClamp, screenUV, 0);
    float4 g2Refract  = gGBuffer2.SampleLevel(gPointClamp, refractUV, 0);
    float straightValid = (length(g2Straight.xyz) > 0.01f) ? 1.0f : 0.0f;
    float refractValid  = (length(g2Refract.xyz)  > 0.01f) ? 1.0f : 0.0f;

    // Discard pixels where the terrain at THIS screen pixel sits above the
    // water surface. Use the straight (non-refracted) sample so we judge the
    // pixel we're actually drawing, not a neighbouring one. This eliminates
    // water bleed onto land caused by water-mesh quads extending past the
    // shoreline.
    float straightDepth = input.worldPos.y - g2Straight.xyz.y;
    if (straightValid > 0.5f && straightDepth < 0.06f) discard;

    // Refraction uses its own sample; if invalid (e.g. sky), fall back to straight.
    float3 floorWorld   = (refractValid > 0.5f) ? g2Refract.xyz : g2Straight.xyz;
    float  gbufferValid = max(refractValid, straightValid);

    float proceduralDepth = pow(saturate(input.shore), 1.15f) * depthFade;
    float realDepth       = max(input.worldPos.y - floorWorld.y, 0.0f);
    float waterDepth      = lerp(proceduralDepth, realDepth, gbufferValid);

    float ndotvFlat = saturate(dot(float3(0.0f, 1.0f, 0.0f), viewDir));
    float opticalDepth = waterDepth / max(ndotvFlat, 0.18f);
    float3 absorption = float3(0.36f, 0.11f, 0.065f);
    float3 transmittance = exp(-absorption * opticalDepth);

    float3 shallowCol = lerp(float3(0.25f, 0.56f, 0.62f), shallowColor.rgb, 0.65f);
    float3 deepCol = lerp(float3(0.012f, 0.07f, 0.15f), deepColor.rgb, 0.75f);
    float3 waterCol = lerp(shallowCol, deepCol, saturate(opticalDepth / max(depthFade, 0.001f)));

    float3 refractedScene = waterCol;
    if (waterFeatureFlags.x > 0.5f) {
        float refractWeight = saturate(opticalDepth * 0.18f) * refractValid * shoreSafe;
        float2 finalUV = lerp(screenUV, refractUV, refractWeight);
        float3 sceneCol = gPrevScene.SampleLevel(gLinearClamp, finalUV, 0).rgb;
        float3 refractedColor = sceneCol * transmittance + waterCol * (1.0f - transmittance);
        refractedScene = lerp(waterCol, refractedColor, gbufferValid);
    }

    // Caustics belong on the underwater floor, fade out as depth grows
    // (light doesn't reach far down) and as we approach the deep open water.
    float2 causticUV = (gbufferValid > 0.5f) ? floorWorld.xz : input.worldPos.xz;
    float caustic = Caustics(causticUV, time);
    // Only show caustics in shallow-to-mid water and only where we actually
    // have refracted scene data; far from camera fade them out entirely.
    float causticFade = transmittance.g * (1.0f - saturate(opticalDepth * 0.25f)) * farFade * gbufferValid;
    refractedScene += float3(0.36f, 0.46f, 0.40f) * caustic * causticFade * 0.18f;

    float ndotv = saturate(dot(normal, viewDir));
    float fresnel = SchlickFresnel(ndotv, 0.022f);
    float3 sky = ProceduralSky(reflect(-viewDir, normal), lightDir);

    float3 halfDir = normalize(lightDir + viewDir);
    // Soft, wide specular -- avoids per-wave-crest line highlights.
    float specTight = pow(saturate(dot(normal, halfDir)), 48.0f) * 1.4f;
    float specWide  = pow(saturate(dot(normal, halfDir)), 14.0f) * 0.18f * saturate(dot(normal, lightDir));

    // Pond / lake aesthetic: no whitecap crest foam (only oceans break visibly).
    // Keep shore foam for the gentle lapping look at the waterline.
    float foamNoise = Fbm3(input.worldPos.xz * 0.42f + float2(time * 0.04f, time * 0.06f));
    float shoreFoam = (1.0f - smoothstep(0.0f, 0.58f, waterDepth)) * (0.30f + foamNoise * 0.55f);
    float foam = saturate(shoreFoam);

    float skyMix = fresnel * smoothstep(0.18f, 0.62f, input.shore);
    float3 color = lerp(refractedScene, sky, skyMix);
    color += float3(1.0f, 0.96f, 0.84f) * (specTight + specWide) * farFade;
    color = lerp(color, float3(0.96f, 0.985f, 1.0f), foam * 0.82f);

    // Alpha: fade in slowly from the shore so the transition against sky
    // (visible through partial-alpha water) is below 1px detectable.
    float softEdge   = saturate(waterDepth * 0.78f);
    float shoreFade  = smoothstep(0.06f, 0.45f, input.shore);
    // Damp sky reflection in the near-shore band too -- otherwise the
    // partial-alpha pixels show a sky-tinted color against the actual sky.
    float skyDamp = smoothstep(0.06f, 0.50f, input.shore);
    float alpha = saturate((0.55f + softEdge * 0.32f + fresnel * 0.16f * skyDamp + foam * 0.34f) * shoreFade);
    return float4(color, alpha);
}
