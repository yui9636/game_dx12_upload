// ============================================================================
// SoA Particle Layout for DX12 Compute Particle Overhaul v3
// Billboard: Hot(32B) + Warm(16B) + Cold(32B) + Header(8B) = 88B/particle
// ============================================================================

#ifndef EFFECT_PARTICLE_SOA_HLSLI
#define EFFECT_PARTICLE_SOA_HLSLI

// ── Billboard Hot Stream (32 bytes, updated every frame) ──
// position.xyz + half-packed age/remainLife + velocity.xyz + half-packed size/spin
struct BillboardHot
{
    float3 position;        // 12B
    uint   ageLifePacked;   //  4B  age(f16) | remainLife(f16)
    float3 velocity;        // 12B
    uint   sizeSpin;        //  4B  currentSize(f16) | spinAngle(f16)
};

// ── Billboard Warm Stream (16 bytes, read at render) ──
struct BillboardWarm
{
    uint   packedColor;     //  4B  RGBA8_UNORM
    uint   packedEndColor;  //  4B  RGBA8_UNORM (lerp target)
    uint   texcoordPacked;  //  4B  u(f16) | v(f16) (atlas rect)
    uint   flags;           //  4B  blendMode(2), sortMode(2), material(8), subUvFrame(8), soft(1), ...
};

// ── Billboard Cold Stream (32 bytes, written once at emit) ──
struct BillboardCold
{
    float3 acceleration;    // 12B
    uint   dragSpinPacked;  //  4B  drag(f16) | spinRate(f16)
    uint   sizeRange;       //  4B  startSize(f16) | endSize(f16)
    uint   lifeBias;        //  4B  totalLife(f16) | alphaBias(f16)
    uint   sizeFadeBias;    //  4B  sizeBias(f16) | fadeBias(f16)
    uint   emitterSeed;     //  4B
};

// ── Billboard Header (8 bytes) ──
struct BillboardHeader
{
    uint   slotIndex;       //  4B
    uint   packed;          //  4B  alive(1) | depthBin(5) | pageHandle(16) | rendererBin(10)
};

// ── Header bit packing helpers ──
static const uint HEADER_ALIVE_BIT       = 0x80000000u;
static const uint HEADER_DEPTHBIN_SHIFT  = 25u;
static const uint HEADER_DEPTHBIN_MASK   = 0x3E000000u; // 5 bits
static const uint HEADER_PAGE_SHIFT      = 10u;
static const uint HEADER_PAGE_MASK       = 0x03FFFC00u; // 16 bits
static const uint HEADER_BIN_MASK        = 0x000003FFu; // 10 bits

bool HeaderIsAlive(uint packed) { return (packed & HEADER_ALIVE_BIT) != 0; }

uint HeaderGetDepthBin(uint packed) { return (packed >> HEADER_DEPTHBIN_SHIFT) & 0x1Fu; }

uint HeaderGetPage(uint packed) { return (packed >> HEADER_PAGE_SHIFT) & 0xFFFFu; }

uint HeaderGetBin(uint packed) { return packed & HEADER_BIN_MASK; }

uint HeaderPack(bool alive, uint depthBin, uint pageHandle, uint bin)
{
    uint p = 0;
    if (alive) p |= HEADER_ALIVE_BIT;
    p |= (depthBin & 0x1Fu) << HEADER_DEPTHBIN_SHIFT;
    p |= (pageHandle & 0xFFFFu) << HEADER_PAGE_SHIFT;
    p |= (bin & 0x3FFu);
    return p;
}

// ── half-float pack/unpack ──
uint PackHalf2(float a, float b)
{
    return f32tof16(a) | (f32tof16(b) << 16u);
}

float2 UnpackHalf2(uint packed)
{
    return float2(f16tof32(packed), f16tof32(packed >> 16u));
}

// ── RGBA8 pack/unpack ──
uint PackRGBA8(float4 c)
{
    uint r = (uint)(saturate(c.x) * 255.0f + 0.5f);
    uint g = (uint)(saturate(c.y) * 255.0f + 0.5f);
    uint b = (uint)(saturate(c.z) * 255.0f + 0.5f);
    uint a = (uint)(saturate(c.w) * 255.0f + 0.5f);
    return (r) | (g << 8u) | (b << 16u) | (a << 24u);
}

float4 UnpackRGBA8(uint packed)
{
    return float4(
        (float)(packed & 0xFFu) / 255.0f,
        (float)((packed >> 8u) & 0xFFu) / 255.0f,
        (float)((packed >> 16u) & 0xFFu) / 255.0f,
        (float)((packed >> 24u) & 0xFFu) / 255.0f);
}

// ── Counter buffer layout (v3) ──
static const uint COUNTER_ALIVE_BILLBOARD   = 0;
static const uint COUNTER_ALIVE_MESH        = 4;
static const uint COUNTER_ALIVE_RIBBON      = 8;
static const uint COUNTER_ALIVE_TOTAL       = 12;
static const uint COUNTER_ALLOCATED_PAGES   = 16;
static const uint COUNTER_SPARSE_PAGES      = 20;
static const uint COUNTER_OVERFLOW          = 24;
static const uint COUNTER_DROPPED_EMIT      = 28;
static const uint COUNTER_DEAD_STACK_TOP    = 32;


// ── Page metadata (GPU-side) ──
struct GpuPageMeta
{
    uint pageHandle;
    uint state;          // 0=Free, 1=Reserved, 2=Active, 3=Sparse, 4=ReclaimPending
    uint ownerEmitter;
    uint rendererClass;  // 0=Billboard, 1=Mesh, 2=Ribbon
    uint liveCount;
    uint baseSlot;       // arena offset = handle * PAGE_SIZE
    uint nextPage;
    uint flags;
};

static const uint PAGE_SIZE = 8192u;

static const uint PAGE_STATE_FREE            = 0;
static const uint PAGE_STATE_RESERVED        = 1;
static const uint PAGE_STATE_ACTIVE          = 2;
static const uint PAGE_STATE_SPARSE          = 3;
static const uint PAGE_STATE_RECLAIM_PENDING = 4;

// ── CoarseDepthBin ──
static const uint DEPTH_BIN_COUNT = 32u;

uint ComputeDepthBin(float viewZ, float nearClip, float farClip)
{
    float depth = max(viewZ, nearClip);
    float logNear = log2(nearClip);
    float logFar  = log2(farClip);
    float depthNorm = (log2(depth) - logNear) / (logFar - logNear);
    return clamp((uint)(depthNorm * (float)DEPTH_BIN_COUNT), 0u, DEPTH_BIN_COUNT - 1u);
}

#endif // EFFECT_PARTICLE_SOA_HLSLI
