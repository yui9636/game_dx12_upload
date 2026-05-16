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

struct VS_INPUT
{
    float3 position  : POSITION;
    float2 uv        : TEXCOORD0;
    float2 shoreData : TEXCOORD1;
};

struct VS_OUTPUT
{
    float4 position   : SV_Position;
    float3 worldPos   : TEXCOORD0;
    float2 uv         : TEXCOORD1;
    float  shore      : TEXCOORD2;
    float3 baseNormal : TEXCOORD3;
    float  crest      : TEXCOORD4;
};

float3 AccumulatePondWaves(float2 worldXZ, float time, float waveScale, float waveSpeed,
                           out float3 tangent, out float3 binormal, out float crestOut)
{
    const float2 dirs[5] = {
        float2( 0.86f,  0.51f),
        float2(-0.64f,  0.77f),
        float2( 0.18f, -0.98f),
        float2(-0.95f, -0.31f),
        float2( 0.52f,  0.85f)
    };
    const float wavelengths[5] = { 18.0f, 11.0f, 6.5f, 3.2f, 1.7f };
    const float amplitudes[5]  = { 0.16f, 0.10f, 0.055f, 0.030f, 0.016f };
    const float speeds[5]      = { 0.42f, 0.58f, 0.74f, 0.96f, 1.20f };

    tangent = float3(1.0f, 0.0f, 0.0f);
    binormal = float3(0.0f, 0.0f, 1.0f);
    float3 disp = 0.0f;
    float crest = 0.0f;
    float ampScale = max(waveScale * 16.0f, 0.015f);

    [unroll]
    for (int i = 0; i < 5; ++i) {
        float2 d = normalize(dirs[i]);
        float k = 6.2831853f / wavelengths[i];
        float phase = k * dot(d, worldXZ) - time * speeds[i] * waveSpeed;
        float amp = amplitudes[i] * ampScale;
        float sinF = sin(phase);
        float cosF = cos(phase);

        disp.xz += d * (cosF * amp * 0.22f);
        disp.y += sinF * amp;

        float slope = amp * k;
        tangent += float3(-d.x * d.x * slope * sinF, d.x * slope * cosF, -d.x * d.y * slope * sinF);
        binormal += float3(-d.x * d.y * slope * sinF, d.y * slope * cosF, -d.y * d.y * slope * sinF);
        crest += saturate(sinF * slope * 3.5f);
    }

    crestOut = saturate(crest * 0.55f);
    return disp;
}

VS_OUTPUT main(VS_INPUT input)
{
    VS_OUTPUT output;
    float3 worldPos = input.position + worldOffsetSeaLevel.xyz;
    float shore = saturate(input.shoreData.x);
    float waveMask = smoothstep(0.02f, 0.38f, shore);

    float3 tangent;
    float3 binormal;
    float crest;
    float3 disp = AccumulatePondWaves(worldPos.xz, params.x, params.z, params.y, tangent, binormal, crest);
    worldPos += disp * waveMask;

    float3 normal = normalize(cross(binormal, tangent));
    if (normal.y < 0.0f) {
        normal = -normal;
    }

    output.position = mul(float4(worldPos, 1.0f), viewProj);
    output.worldPos = worldPos;
    output.uv = input.uv;
    output.shore = shore;
    output.baseNormal = normal;
    output.crest = crest * waveMask;
    return output;
}
