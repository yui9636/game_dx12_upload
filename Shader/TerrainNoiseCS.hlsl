cbuffer Params : register(b0)
{
    int   resolution;
    int   erosionIterations;
    int   erosionRadius;
    int   erosionLifetime;

    float noiseFreq;
    int   octaves;
    float lacunarity;
    float gain;

    int   noiseSeed;
    int   erosionSeed;
    float inertia;
    float sedimentCapacityFactor;

    float minSedimentCapacity;
    float erodeSpeed;
    float depositSpeed;
    float evaporateSpeed;

    float gravity;
    float initialWater;
    float initialSpeed;
    float rockAltitudeMin;

    float rockSlopeDegrees;
    float dirtMidAltitude;
    float dirtStrength;
    float heightScale;

    float worldSizeX;
    float worldSizeZ;
    int   noiseType;
    int   padding0;

    float domainWarpStrength;
    float terraceSteps;
    int   padding1;
    int   padding2;
};

RWStructuredBuffer<float> heightBuf : register(u0);
RWStructuredBuffer<uint>  splatBuf  : register(u1);

// =======================================================================
// 2D Simplex (Ashima / Stefan Gustavson). Returns ~[-1, 1].
// =======================================================================
float3 sn_mod289(float3 x) { return x - floor(x * (1.0f / 289.0f)) * 289.0f; }
float2 sn_mod289(float2 x) { return x - floor(x * (1.0f / 289.0f)) * 289.0f; }
float3 sn_permute(float3 x) { return sn_mod289(((x * 34.0f) + 1.0f) * x); }

float Simplex2D(float2 v)
{
    const float4 C = float4( 0.211324865405187f, 0.366025403784439f,
                            -0.577350269189626f, 0.024390243902439f );
    float2 i  = floor(v + dot(v, C.yy));
    float2 x0 = v - i + dot(i, C.xx);
    float2 i1 = (x0.x > x0.y) ? float2(1.0f, 0.0f) : float2(0.0f, 1.0f);
    float4 x12 = x0.xyxy + C.xxzz;
    x12.xy -= i1;
    i = sn_mod289(i);
    float3 p = sn_permute(sn_permute(i.y + float3(0.0f, i1.y, 1.0f))
                        + i.x + float3(0.0f, i1.x, 1.0f));
    float3 m = max(0.5f - float3(dot(x0, x0), dot(x12.xy, x12.xy), dot(x12.zw, x12.zw)), 0.0f);
    m = m * m; m = m * m;
    float3 x = 2.0f * frac(p * C.www) - 1.0f;
    float3 h = abs(x) - 0.5f;
    float3 ox = floor(x + 0.5f);
    float3 a0 = x - ox;
    m *= 1.79284291400159f - 0.85373472095314f * (a0 * a0 + h * h);
    float3 g;
    g.x  = a0.x  * x0.x  + h.x  * x0.y;
    g.yz = a0.yz * x12.xz + h.yz * x12.yw;
    return 130.0f * dot(m, g);
}

// =======================================================================
// 2D Cellular (Worley F1). Returns distance to nearest jittered cell point.
// 0 = right at the point, ~1 = far between. Animated points are not used here.
// =======================================================================
float Hash21(float2 p)
{
    p = frac(p * float2(123.34f, 456.21f));
    p += dot(p, p + 45.32f);
    return frac(p.x * p.y);
}

float Cellular(float2 p)
{
    float2 i = floor(p);
    float2 f = frac(p);
    float minDist = 1.5f;
    [unroll]
    for (int dy = -1; dy <= 1; ++dy) {
        [unroll]
        for (int dx = -1; dx <= 1; ++dx) {
            float2 g = float2((float)dx, (float)dy);
            float h = Hash21(i + g);
            float2 cellPoint = g + 0.5f + 0.5f * float2(cos(h * 6.2831853f), sin(h * 6.2831853f));
            float d = length(cellPoint - f);
            minDist = min(minDist, d);
        }
    }
    return saturate(minDist);
}

// =======================================================================
// FBM / Ridge over Simplex.
// =======================================================================
float Fbm(float2 p, int oct, float lac, float gn)
{
    float v = 0.0f;
    float amp = 1.0f;
    float weight = 0.0f;
    [unroll(8)]
    for (int o = 0; o < oct; ++o) {
        v += (Simplex2D(p) * 0.5f + 0.5f) * amp;
        weight += amp;
        p *= lac;
        amp *= gn;
    }
    return saturate(v / max(weight, 0.0001f));
}

float Ridge(float2 p, int oct, float lac, float gn)
{
    float v = 0.0f;
    float amp = 1.0f;
    float weight = 0.0f;
    [unroll(8)]
    for (int o = 0; o < oct; ++o) {
        float n = Simplex2D(p);
        float r = 1.0f - abs(n);
        r = r * r;
        v += r * amp;
        weight += amp;
        p *= lac;
        amp *= gn;
    }
    return saturate(v / max(weight, 0.0001f));
}

// Cellular FBM (2-octave - cellular is expensive, more octaves rarely help).
float CellularFbm(float2 p)
{
    float v = Cellular(p) * 0.6f + Cellular(p * 2.7f + 1.3f) * 0.3f + Cellular(p * 5.1f + 4.9f) * 0.15f;
    return saturate(v / 1.05f);
}

// =======================================================================
// Recursive 2-level Domain Warp. Eliminates underlying noise grid structure.
// =======================================================================
float2 DomainWarp(float2 p, float strength)
{
    float2 q = float2(Simplex2D(p), Simplex2D(p + float2(5.2f, 1.3f)));
    float2 r = float2(Simplex2D(p + 4.0f * q + float2(1.7f, 9.2f)),
                      Simplex2D(p + 4.0f * q + float2(8.3f, 2.8f)));
    return p + r * strength;
}

[numthreads(8, 8, 1)]
void main(uint3 tid : SV_DispatchThreadID)
{
    if ((int)tid.x >= resolution || (int)tid.y >= resolution) return;

    // World-space coordinates so noiseFreq behaves as cycles per meter.
    float cellX = worldSizeX / max((float)(resolution - 1), 1.0f);
    float cellZ = worldSizeZ / max((float)(resolution - 1), 1.0f);
    float2 base = float2((float)tid.x * cellX, (float)tid.y * cellZ)
                + float2(noiseSeed * 0.137f, noiseSeed * 0.249f);

    // Warp on the low-frequency layer; strength scales by world meters so
    // the param is intuitive ("how many meters of distortion").
    float2 warpInput = base * (noiseFreq * 0.8f);
    float2 warped    = DomainWarp(warpInput, domainWarpStrength * noiseFreq);
    float2 worldWarped = base + (warped - warpInput) / max(noiseFreq * 0.8f, 0.0001f);

    float h;
    if (noiseType == 2) {
        // ---- Cellular only (crystalline / rocky landscape) ----
        float c1 = CellularFbm(worldWarped * (noiseFreq * 1.0f));
        float c2 = CellularFbm(worldWarped * (noiseFreq * 2.4f));
        h = 0.20f + (1.0f - c1) * 0.55f + (1.0f - c2) * 0.20f;
    }
    else if (noiseType == 1) {
        // ---- Simplex only (smooth rolling terrain) ----
        float continent = Fbm  (worldWarped * (noiseFreq * 0.6f), max(octaves - 1, 1), lacunarity, gain);
        float hills     = Fbm  (worldWarped * (noiseFreq * 2.5f), octaves,             lacunarity, gain);
        float ridges    = Ridge(worldWarped * (noiseFreq * 1.4f), max(octaves - 1, 1), lacunarity, gain);
        float detail    = Fbm  (worldWarped * (noiseFreq * 9.0f), 3, 2.1f, 0.55f);
        h = 0.18f
          + continent * 0.55f
          + (hills - 0.5f) * 0.35f
          + ridges * continent * 0.45f
          + (detail - 0.5f) * 0.08f;
    }
    else {
        // ---- Hybrid (default): Simplex base + Cellular rocky peaks ----
        float continent = Fbm  (worldWarped * (noiseFreq * 0.6f), max(octaves - 1, 1), lacunarity, gain);
        float hills     = Fbm  (worldWarped * (noiseFreq * 2.5f), octaves,             lacunarity, gain);
        float ridges    = Ridge(worldWarped * (noiseFreq * 1.4f), max(octaves - 1, 1), lacunarity, gain);
        float rocky     = 1.0f - CellularFbm(worldWarped * (noiseFreq * 3.2f));  // peaks at cell centers
        float detail    = Fbm  (worldWarped * (noiseFreq * 9.0f), 3, 2.1f, 0.55f);
        h = 0.16f
          + continent * 0.48f
          + (hills - 0.5f) * 0.28f
          + ridges  * continent * 0.30f
          + rocky   * continent * 0.20f
          + (detail - 0.5f) * 0.06f;
    }

    // Soft basin in the lower 30% for a lake-ready depression.
    float basinFloor = 0.22f;
    h += max(basinFloor - h, 0.0f) * 0.55f;
    h = saturate(h);

    // Optional terrace / plateau quantization.
    if (terraceSteps > 0.5f) {
        float t = h * terraceSteps;
        float f = frac(t);
        float step = smoothstep(0.55f, 1.0f, f);
        h = (floor(t) + step) / terraceSteps;
    }

    heightBuf[tid.y * resolution + tid.x] = saturate(h);
}
