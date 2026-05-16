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

float Caustics(float2 worldXZ, float time)
{
    float2 a = worldXZ * 0.18f + float2(time * 0.10f, time * 0.07f);
    float2 b = worldXZ * 0.29f + float2(-time * 0.06f, time * 0.09f);
    float v = (sin(a.x + b.y) * sin(a.y - b.x)) * 0.5f + 0.5f;
    return pow(saturate(v), 2.2f);
}

float4 main(PS_INPUT input) : SV_Target
{
    float time = params.x;
    float waveSpeed = params.y;
    float depthFade = max(params.w, 0.001f);

    float3 ripple = RippleNormal(input.worldPos.xz, time, waveSpeed);
    float3 normal = normalize(input.baseNormal + ripple * 0.35f);
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

    float4 g2Refract = gGBuffer2.SampleLevel(gPointClamp, refractUV, 0);
    float4 g2Straight = gGBuffer2.SampleLevel(gPointClamp, screenUV, 0);
    float refractValid = (length(g2Refract.xyz) > 0.01f) ? 1.0f : 0.0f;
    float straightValid = (length(g2Straight.xyz) > 0.01f) ? 1.0f : 0.0f;
    float3 floorWorld = (refractValid > 0.5f) ? g2Refract.xyz : g2Straight.xyz;
    float gbufferValid = max(refractValid, straightValid);

    float proceduralDepth = pow(saturate(input.shore), 1.15f) * depthFade;
    float realDepth = max(input.worldPos.y - floorWorld.y, 0.0f);
    float waterDepth = lerp(proceduralDepth, realDepth, gbufferValid);

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

    float caustic = Caustics((gbufferValid > 0.5f) ? floorWorld.xz : input.worldPos.xz, time);
    refractedScene += float3(0.42f, 0.56f, 0.48f) * caustic * saturate(transmittance.g * 1.4f) * 0.34f;

    float ndotv = saturate(dot(normal, viewDir));
    float fresnel = SchlickFresnel(ndotv, 0.022f);
    float3 sky = ProceduralSky(reflect(-viewDir, normal), lightDir);

    float3 halfDir = normalize(lightDir + viewDir);
    float specTight = pow(saturate(dot(normal, halfDir)), 180.0f) * 4.8f;
    float specWide = pow(saturate(dot(normal, halfDir)), 18.0f) * 0.20f * saturate(dot(normal, lightDir));

    float foamNoise = Fbm3(input.worldPos.xz * 0.42f + float2(time * 0.04f, time * 0.06f));
    float shoreFoam = (1.0f - smoothstep(0.0f, 0.58f, waterDepth)) * (0.35f + foamNoise * 0.65f);
    float crestFoam = smoothstep(0.58f, 0.95f, input.crest) * (0.45f + foamNoise * 0.55f);
    float foam = saturate(max(shoreFoam, crestFoam));

    float skyMix = fresnel * smoothstep(0.18f, 0.62f, input.shore);
    float3 color = lerp(refractedScene, sky, skyMix);
    color += float3(1.0f, 0.96f, 0.84f) * (specTight + specWide) * farFade;
    color = lerp(color, float3(0.96f, 0.985f, 1.0f), foam * 0.82f);

    float softEdge = saturate(waterDepth * 0.78f);
    float shoreCoverage = 1.0f - smoothstep(0.0f, 0.34f, waterDepth);
    float alpha = saturate(0.58f + softEdge * 0.30f + fresnel * 0.16f + foam * 0.34f + shoreCoverage * 0.24f);
    return float4(color, alpha);
}
