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

uint PackRGBA(float r, float g, float b, float a)
{
    uint ri = (uint)(saturate(r) * 255.0f);
    uint gi = (uint)(saturate(g) * 255.0f);
    uint bi = (uint)(saturate(b) * 255.0f);
    uint ai = (uint)(saturate(a) * 255.0f);
    return ri | (gi << 8u) | (bi << 16u) | (ai << 24u);
}

[numthreads(8, 8, 1)]
void main(uint3 tid : SV_DispatchThreadID)
{
    if ((int)tid.x >= resolution || (int)tid.y >= resolution) return;
    int x = (int)tid.x;
    int y = (int)tid.y;

    int xL = max(x - 1, 0);
    int xR = min(x + 1, resolution - 1);
    int yD = max(y - 1, 0);
    int yU = min(y + 1, resolution - 1);

    float h  = heightBuf[y * resolution + x];
    float hL = heightBuf[y * resolution + xL] * heightScale;
    float hR = heightBuf[y * resolution + xR] * heightScale;
    float hD = heightBuf[yD * resolution + x] * heightScale;
    float hU = heightBuf[yU * resolution + x] * heightScale;

    float cellX = worldSizeX / max((float)(resolution - 1), 1.0f);
    float cellZ = worldSizeZ / max((float)(resolution - 1), 1.0f);
    float gx = (hR - hL) / (2.0f * cellX);
    float gz = (hU - hD) / (2.0f * cellZ);
    float slope = atan(sqrt(gx * gx + gz * gz)) * 57.29577951f;

    float rockA = smoothstep(rockAltitudeMin - 0.05f, rockAltitudeMin + 0.05f, h);
    float rockS = smoothstep(rockSlopeDegrees - 5.0f, rockSlopeDegrees + 5.0f, slope);
    float rock  = max(rockA, rockS);
    float dirt  = smoothstep(dirtMidAltitude - 0.10f, dirtMidAltitude + 0.10f, h) * dirtStrength * (1.0f - rock);
    float grass = max(1.0f - rock - dirt, 0.0f);

    splatBuf[y * resolution + x] = PackRGBA(grass, dirt, rock, 0.0f);
}
