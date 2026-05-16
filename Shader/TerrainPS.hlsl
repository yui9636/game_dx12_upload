Texture2D gSplatMap : register(t0);
Texture2D gAlbedo0  : register(t1);
Texture2D gAlbedo1  : register(t2);
Texture2D gAlbedo2  : register(t3);
Texture2D gNormal0  : register(t4);
Texture2D gNormal1  : register(t5);
Texture2D gNormal2  : register(t6);
Texture2D gMRA0     : register(t7);
Texture2D gMRA1     : register(t8);
Texture2D gMRA2     : register(t9);
SamplerState gLinearWrap : register(s0);

cbuffer TerrainCB : register(b0)
{
    float4x4 viewProj;
    float4x4 viewProjectionUnjittered;
    float4x4 prevViewProjection;
    float4   chunkOffset;
    float    heightScale;
    float3   pad;
};

cbuffer TerrainMaterialCB : register(b1)
{
    float4 tileScales;
    float4 triplanarParams;
};

struct PS_INPUT
{
    float4 position    : SV_Position;
    float3 worldPos    : TEXCOORD0;
    float3 normal      : TEXCOORD1;
    float2 uv          : TEXCOORD2;
    float4 curClipPos  : TEXCOORD3;
    float4 prevClipPos : TEXCOORD4;
};

struct PS_OUTPUT
{
    float4 albedoMetallic  : SV_TARGET0;
    float4 normalRoughness : SV_TARGET1;
    float4 worldPosDepth   : SV_TARGET2;
    float2 velocity        : SV_TARGET3;
};

struct TriplanarUVs
{
    float2 xPlane;
    float2 yPlane;
    float2 zPlane;
    float3 weights;
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

float Fbm2(float2 p)
{
    float v = 0.0f;
    float a = 0.5f;
    [unroll]
    for (int i = 0; i < 4; ++i) {
        v += ValueNoise(p) * a;
        p *= 2.02f;
        a *= 0.52f;
    }
    return v;
}

TriplanarUVs ComputeTriplanar(float3 worldPos, float3 normal, float tileScale, float strength)
{
    TriplanarUVs t;
    float scale = max(tileScale, 0.001f) * 0.045f;
    t.xPlane = worldPos.zy * scale;
    t.yPlane = worldPos.xz * scale;
    t.zPlane = worldPos.xy * scale;

    float blendPower = lerp(2.0f, 8.0f, saturate(strength));
    float3 w = pow(abs(normal), blendPower);
    w.y *= lerp(1.0f, 2.35f, saturate(strength));
    t.weights = w / max(w.x + w.y + w.z, 0.0001f);
    return t;
}

float3 SampleAlbedoTriplanar(Texture2D tex, TriplanarUVs t)
{
    float3 ax = tex.Sample(gLinearWrap, t.xPlane).rgb;
    float3 ay = tex.Sample(gLinearWrap, t.yPlane).rgb;
    float3 az = tex.Sample(gLinearWrap, t.zPlane).rgb;
    return ax * t.weights.x + ay * t.weights.y + az * t.weights.z;
}

float3 SampleMRATriplanar(Texture2D tex, TriplanarUVs t)
{
    float3 ax = tex.Sample(gLinearWrap, t.xPlane).rgb;
    float3 ay = tex.Sample(gLinearWrap, t.yPlane).rgb;
    float3 az = tex.Sample(gLinearWrap, t.zPlane).rgb;
    return ax * t.weights.x + ay * t.weights.y + az * t.weights.z;
}

float3 DecodeNormal(Texture2D tex, float2 uv)
{
    float3 n = tex.Sample(gLinearWrap, uv).xyz * 2.0f - 1.0f;
    return normalize(n);
}

float3 SampleNormalTriplanar(Texture2D tex, TriplanarUVs t, float3 worldNormal)
{
    float3 nx = DecodeNormal(tex, t.xPlane);
    float3 ny = DecodeNormal(tex, t.yPlane);
    float3 nz = DecodeNormal(tex, t.zPlane);

    float sx = (worldNormal.x < 0.0f) ? -1.0f : 1.0f;
    float sz = (worldNormal.z < 0.0f) ? -1.0f : 1.0f;

    float3 xWorld = normalize(float3(nx.z * sx, nx.y, nx.x));
    float3 yWorld = normalize(float3(ny.x, ny.z, ny.y));
    float3 zWorld = normalize(float3(nz.x, nz.y, nz.z * sz));
    return normalize(xWorld * t.weights.x + yWorld * t.weights.y + zWorld * t.weights.z);
}

PS_OUTPUT main(PS_INPUT input)
{
    PS_OUTPUT output;

    float3 worldNormal = normalize(input.normal);
    float4 splat = gSplatMap.Sample(gLinearWrap, input.uv);
    float3 weights = max(splat.rgb, 0.0f);
    float weightSum = weights.r + weights.g + weights.b;
    weights = (weightSum > 0.0001f) ? (weights / weightSum) : float3(1.0f, 0.0f, 0.0f);

    float triStrength = saturate(triplanarParams.x);
    TriplanarUVs tri0 = ComputeTriplanar(input.worldPos, worldNormal, tileScales.x, triStrength);
    TriplanarUVs tri1 = ComputeTriplanar(input.worldPos, worldNormal, tileScales.y, triStrength);
    TriplanarUVs tri2 = ComputeTriplanar(input.worldPos, worldNormal, tileScales.z, triStrength);

    float3 albedo0 = SampleAlbedoTriplanar(gAlbedo0, tri0);
    float3 albedo1 = SampleAlbedoTriplanar(gAlbedo1, tri1);
    float3 albedo2 = SampleAlbedoTriplanar(gAlbedo2, tri2);

    float height0 = dot(albedo0, float3(0.30f, 0.59f, 0.11f)) * 0.35f + weights.r * 1.55f;
    float height1 = dot(albedo1, float3(0.30f, 0.59f, 0.11f)) * 0.35f + weights.g * 1.55f;
    float height2 = dot(albedo2, float3(0.30f, 0.59f, 0.11f)) * 0.35f + weights.b * 1.55f;
    float blendRange = 0.26f;
    float maxHeight = max(height0, max(height1, height2));
    float w0 = max(height0 - maxHeight + blendRange, 0.0f);
    float w1 = max(height1 - maxHeight + blendRange, 0.0f);
    float w2 = max(height2 - maxHeight + blendRange, 0.0f);
    float hsum = max(w0 + w1 + w2, 0.0001f);
    float3 blendWeights = float3(w0, w1, w2) / hsum;

    float3 albedoSrgb = albedo0 * blendWeights.r + albedo1 * blendWeights.g + albedo2 * blendWeights.b;

    float macro = Fbm2(input.worldPos.xz * 0.018f);
    float micro = Fbm2(input.worldPos.xz * 0.115f + 17.3f);
    albedoSrgb *= lerp(0.86f, 1.13f, macro);
    albedoSrgb *= lerp(0.94f, 1.06f, micro);

    float3 wetTint = float3(0.58f, 0.56f, 0.48f);
    float3 rockTint = float3(0.82f, 0.86f, 0.88f);
    albedoSrgb = lerp(albedoSrgb, albedoSrgb * wetTint, saturate(weights.g * 0.32f));
    albedoSrgb = lerp(albedoSrgb, albedoSrgb * rockTint, saturate(weights.b * 0.22f));

    float3 n0 = SampleNormalTriplanar(gNormal0, tri0, worldNormal);
    float3 n1 = SampleNormalTriplanar(gNormal1, tri1, worldNormal);
    float3 n2 = SampleNormalTriplanar(gNormal2, tri2, worldNormal);
    float3 detailNormal = normalize(n0 * blendWeights.r + n1 * blendWeights.g + n2 * blendWeights.b);
    float normalStrength = lerp(0.35f, 0.78f, saturate(weights.b + weights.g * 0.45f));
    float3 finalNormal = normalize(lerp(worldNormal, detailNormal, normalStrength));

    float3 mra0 = SampleMRATriplanar(gMRA0, tri0);
    float3 mra1 = SampleMRATriplanar(gMRA1, tri1);
    float3 mra2 = SampleMRATriplanar(gMRA2, tri2);
    float3 mra = mra0 * blendWeights.r + mra1 * blendWeights.g + mra2 * blendWeights.b;
    float metallic = saturate(mra.r);
    float roughness = saturate((mra.g > 0.001f) ? mra.g : lerp(0.88f, 0.72f, weights.b));
    float ao = saturate((mra.b > 0.001f) ? mra.b : 1.0f);

    float2 currentNDC = input.curClipPos.xy / input.curClipPos.w;
    float2 prevNDC = input.prevClipPos.xy / input.prevClipPos.w;
    float2 currentUV = currentNDC * float2(0.5f, -0.5f) + 0.5f;
    float2 prevUV = prevNDC * float2(0.5f, -0.5f) + 0.5f;

    float3 albedoLin = pow(saturate(albedoSrgb), 2.2f);
    output.albedoMetallic = float4(saturate(albedoLin * ao), metallic);
    output.normalRoughness = float4(finalNormal, roughness);
    output.worldPosDepth = float4(input.worldPos, input.position.z);
    output.velocity = prevUV - currentUV;
    return output;
}
