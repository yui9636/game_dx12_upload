// One thread = one droplet. Writes are non-atomic (race-tolerant since
// erosion is stochastic; some lost updates are visually negligible at 60k+
// droplets).

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

float Hash11(uint n)
{
    n = (n << 13U) ^ n;
    n = n * (n * n * 15731U + 789221U) + 1376312589U;
    return (float)(n & 0x7fffffffU) / 2147483647.0f;
}

[numthreads(64, 1, 1)]
void main(uint3 tid : SV_DispatchThreadID)
{
    uint dropletIdx = tid.x;
    if ((int)dropletIdx >= erosionIterations) return;

    uint seed = (uint)erosionSeed;
    float rx = Hash11(dropletIdx * 2u + seed);
    float ry = Hash11(dropletIdx * 2u + 1u + seed * 17u);
    float posX = rx * (float)(resolution - 2);
    float posY = ry * (float)(resolution - 2);
    float dirX = 0.0f;
    float dirY = 0.0f;
    float speed    = initialSpeed;
    float water    = initialWater;
    float sediment = 0.0f;

    for (int life = 0; life < erosionLifetime; ++life) {
        int cx = (int)posX;
        int cy = (int)posY;
        float fx = posX - (float)cx;
        float fy = posY - (float)cy;
        int idxNW = cy * resolution + cx;
        float hNW = heightBuf[idxNW];
        float hNE = heightBuf[idxNW + 1];
        float hSW = heightBuf[idxNW + resolution];
        float hSE = heightBuf[idxNW + resolution + 1];

        float gx = (hNE - hNW) * (1.0f - fy) + (hSE - hSW) * fy;
        float gy = (hSW - hNW) * (1.0f - fx) + (hSE - hNE) * fx;
        float h  = hNW * (1.0f - fx) * (1.0f - fy)
                 + hNE * fx          * (1.0f - fy)
                 + hSW * (1.0f - fx) * fy
                 + hSE * fx          * fy;

        dirX = dirX * inertia - gx * (1.0f - inertia);
        dirY = dirY * inertia - gy * (1.0f - inertia);
        float len = sqrt(dirX * dirX + dirY * dirY);
        if (len > 0.0001f) { dirX /= len; dirY /= len; }
        float newPosX = posX + dirX;
        float newPosY = posY + dirY;
        if (newPosX < 1.0f || newPosX >= (float)(resolution - 2) ||
            newPosY < 1.0f || newPosY >= (float)(resolution - 2) ||
            (dirX == 0.0f && dirY == 0.0f)) {
            break;
        }

        int ncx = (int)newPosX;
        int ncy = (int)newPosY;
        float nfx = newPosX - (float)ncx;
        float nfy = newPosY - (float)ncy;
        int nidx = ncy * resolution + ncx;
        float nh = heightBuf[nidx] * (1.0f - nfx) * (1.0f - nfy)
                 + heightBuf[nidx + 1] * nfx * (1.0f - nfy)
                 + heightBuf[nidx + resolution] * (1.0f - nfx) * nfy
                 + heightBuf[nidx + resolution + 1] * nfx * nfy;
        float deltaHeight = nh - h;
        float capacity = max(-deltaHeight * speed * water * sedimentCapacityFactor,
                             minSedimentCapacity);

        if (sediment > capacity || deltaHeight > 0.0f) {
            // Deposit at current 4 corners (bilinear).
            float depositAmount = (deltaHeight > 0.0f)
                ? min(deltaHeight, sediment)
                : (sediment - capacity) * depositSpeed;
            sediment -= depositAmount;
            heightBuf[idxNW]                  += depositAmount * (1.0f - fx) * (1.0f - fy);
            heightBuf[idxNW + 1]              += depositAmount * fx          * (1.0f - fy);
            heightBuf[idxNW + resolution]     += depositAmount * (1.0f - fx) * fy;
            heightBuf[idxNW + resolution + 1] += depositAmount * fx          * fy;
        } else {
            float erodeAmount = min((capacity - sediment) * erodeSpeed, -deltaHeight);
            // Erosion brush: triangular falloff in a (2R+1)^2 box.
            float weightSum = 0.0f;
            for (int dy = -erosionRadius; dy <= erosionRadius; ++dy) {
                for (int dx = -erosionRadius; dx <= erosionRadius; ++dx) {
                    int bx = cx + dx;
                    int by = cy + dy;
                    if (bx < 0 || by < 0 || bx >= resolution || by >= resolution) continue;
                    float dist = sqrt((float)(dx * dx + dy * dy));
                    weightSum += max(0.0f, (float)erosionRadius - dist);
                }
            }
            if (weightSum > 0.0001f) {
                float invWeight = 1.0f / weightSum;
                for (int dy2 = -erosionRadius; dy2 <= erosionRadius; ++dy2) {
                    for (int dx2 = -erosionRadius; dx2 <= erosionRadius; ++dx2) {
                        int bx2 = cx + dx2;
                        int by2 = cy + dy2;
                        if (bx2 < 0 || by2 < 0 || bx2 >= resolution || by2 >= resolution) continue;
                        float dist2 = sqrt((float)(dx2 * dx2 + dy2 * dy2));
                        float w = max(0.0f, (float)erosionRadius - dist2) * invWeight;
                        heightBuf[by2 * resolution + bx2] -= erodeAmount * w;
                    }
                }
            }
            sediment += erodeAmount;
        }
        speed = sqrt(max(0.0f, speed * speed + deltaHeight * gravity));
        water *= (1.0f - evaporateSpeed);
        posX = newPosX;
        posY = newPosY;
    }
}
