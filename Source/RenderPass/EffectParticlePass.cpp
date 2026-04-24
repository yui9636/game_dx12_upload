#include "EffectParticlePass.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <unordered_map>
#include <vector>

#include <d3d12.h>
#include <d3dcompiler.h>
#include <wrl/client.h>

#include "Console/Logger.h"
#include <cstdio>
#define PARTICLE_LOG(...) do { char _buf[512]; snprintf(_buf, sizeof(_buf), __VA_ARGS__); OutputDebugStringA(_buf); OutputDebugStringA("\n"); Logger::Instance().Print(LogLevel::Info, __VA_ARGS__); } while(0)
#include "EffectRuntime/EffectGraphAsset.h"
#include "Graphics.h"
#include "Model/ModelResource.h"
#include "FastNoiseLite.h"
#include "RHI/IBuffer.h"
#include "RHI/IResourceFactory.h"
#include "RHI/ITexture.h"
#include "RHI/DX12/DX12Buffer.h"
#include "RHI/DX12/DX12CommandList.h"
#include "RHI/DX12/DX12Device.h"
#include "RHI/DX12/DX12Texture.h"
#include "RenderGraph/FrameGraphBuilder.h"
#include "RenderGraph/FrameGraphResources.h"
#include "ShadowMap.h"
#include "RenderContext/RenderQueue.h"
#include "System/ResourceManager.h"

using Microsoft::WRL::ComPtr;

namespace
{
    constexpr uint32_t kEffectParticleRibbonHistoryLength = 8u;
    constexpr uint32_t kEffectParticlePageSize = 8192u;
    constexpr uint32_t kEffectParticleArenaGrowthPages = 64u;
    constexpr uint32_t kEffectParticleCounterReadbackLatency = 3u;

    // Per-renderer budget caps (spec §0: Tiered Performance Targets)
    constexpr uint32_t kBillboardBudgetCapacity = 2'000'000u;  // Tier 1 must
    constexpr uint32_t kMeshBudgetCapacity      = 250'000u;
    constexpr uint32_t kRibbonBudgetCapacity    = 100'000u;
    // Readback throttling: only readback every N frames per emitter
    constexpr uint32_t kReadbackThrottleInterval = 2u;

    enum class ParticlePageState : uint32_t
    {
        Free = 0,
        Reserved,
        Active,
        Sparse,
        ReclaimPending
    };

    struct GradientColorGpu
    {
        DirectX::XMFLOAT4 color = { 0.0f, 0.0f, 0.0f, 0.0f };
        float time = 0.0f;
        float pad[3] = {};
    };

    struct EffectParticleGpuData
    {
        DirectX::XMFLOAT4 parameter = { 0.0f, 0.0f, 0.0f, 0.0f };
        DirectX::XMFLOAT4 position = { 0.0f, 0.0f, 0.0f, 1.0f };
        DirectX::XMFLOAT4 rotation = { 0.0f, 0.0f, 0.0f, 0.0f };
        DirectX::XMFLOAT4 scale = { 0.0f, 0.0f, 0.0f, 1.0f };
        DirectX::XMFLOAT4 scaleBegin = { 0.0f, 0.0f, 0.0f, 0.0f };
        DirectX::XMFLOAT4 scaleEnd = { 0.0f, 0.0f, 0.0f, 0.0f };
        DirectX::XMFLOAT4 velocity = { 0.0f, 0.0f, 0.0f, 0.0f };
        DirectX::XMFLOAT4 acceleration = { 0.0f, 0.0f, 0.0f, 0.0f };
        DirectX::XMFLOAT4 texcoord = { 0.0f, 0.0f, 1.0f, 1.0f };
        GradientColorGpu gradientColors[4] = {};
        int gradientCount = 0;
        float gradientPad[3] = {};
        DirectX::XMFLOAT4 color = { 1.0f, 1.0f, 1.0f, 1.0f };
        DirectX::XMFLOAT4 angularVelocity = { 0.0f, 0.0f, 0.0f, 0.0f };
        DirectX::XMFLOAT2 fade = { 0.0f, 1.0f };
        float fadePad[2] = {};
    };

    struct EffectParticleGpuHeader
    {
        uint32_t alive = 0;
        uint32_t particleIndex = 0;
        float depth = 0.0f;
        uint32_t dummy = 0;
    };

    struct EffectParticleSimulationConstants
    {
        DirectX::XMFLOAT4 originCurrentTime = { 0.0f, 0.0f, 0.0f, 0.0f };
        DirectX::XMFLOAT4 tint = { 1.0f, 1.0f, 1.0f, 1.0f };
        DirectX::XMFLOAT4 tintEnd = { 1.0f, 1.0f, 1.0f, 0.0f };
        DirectX::XMFLOAT4 cameraPositionSortSign = { 0.0f, 0.0f, 0.0f, 1.0f };
        DirectX::XMFLOAT4 cameraDirectionCapacity = { 0.0f, 0.0f, 1.0f, static_cast<float>(kEffectParticleMinSuggestedMaxParticles) };
        DirectX::XMFLOAT4 accelerationDrag = { 0.0f, -0.55f, 0.0f, 0.0f };
        DirectX::XMFLOAT4 shapeParametersSizeBias = { 0.35f, 0.35f, 0.35f, 1.0f };
        DirectX::XMFLOAT4 shapeTypeSpinAlphaBias = { 1.0f, 6.0f, 1.0f, 0.0f };
        DirectX::XMFLOAT4 timing = { 0.0f, 1.0f, 1.0f, 0.0f };
        DirectX::XMFLOAT4 sizeSeed = { 0.18f, 0.04f, 1.0f, 0.0f };
        DirectX::XMFLOAT4 subUvParams = { 1.0f, 1.0f, 0.0f, 0.0f };
        DirectX::XMFLOAT4 motionParams = { 0.0f, 0.18f, 0.20f, 0.0f };
        DirectX::XMFLOAT4 randomParams = { 0.0f, 0.0f, 0.0f, 0.0f }; // x=speedRange, y=sizeRange, z=lifeRange, w=windStrength
        DirectX::XMFLOAT4 windDirection = { 1.0f, 0.0f, 0.0f, 0.0f }; // xyz=direction, w=turbulence
        // Phase 1C: Size curve (4 keys)
        DirectX::XMFLOAT4 sizeCurveValues = { 0.18f, 0.18f, 0.04f, 0.04f }; // s0,s1,s2,s3
        DirectX::XMFLOAT4 sizeCurveTimes  = { 0.0f,  0.33f, 0.66f, 1.0f };  // t0,t1,t2,t3
        // Phase 1C: Color gradient (4 keys)
        DirectX::XMFLOAT4 gradientColor0 = { 1.0f, 1.0f, 1.0f, 1.0f };
        DirectX::XMFLOAT4 gradientColor1 = { 1.0f, 1.0f, 1.0f, 1.0f };
        DirectX::XMFLOAT4 gradientColor2 = { 1.0f, 1.0f, 1.0f, 0.0f };
        DirectX::XMFLOAT4 gradientColor3 = { 1.0f, 1.0f, 1.0f, 0.0f };
        DirectX::XMFLOAT4 gradientTimes  = { 0.0f, 0.33f, 0.66f, 1.0f }; // t0,t1,t2,t3
        // Phase 2: Attractors
        DirectX::XMFLOAT4 attractor0 = {};
        DirectX::XMFLOAT4 attractor1 = {};
        DirectX::XMFLOAT4 attractor2 = {};
        DirectX::XMFLOAT4 attractor3 = {};
        DirectX::XMFLOAT4 attractorRadii = { 5.0f, 5.0f, 5.0f, 5.0f };
        DirectX::XMFLOAT4 attractorFalloff = { 1.0f, 1.0f, 1.0f, 1.0f };
        // Phase 2: Collision
        DirectX::XMFLOAT4 collisionPlane = { 0.0f, 1.0f, 0.0f, 0.0f };
        DirectX::XMFLOAT4 collisionSphere0 = {};
        DirectX::XMFLOAT4 collisionSphere1 = {};
        DirectX::XMFLOAT4 collisionSphere2 = {};
        DirectX::XMFLOAT4 collisionSphere3 = {};
        DirectX::XMFLOAT4 collisionParams = {}; // x=restitution, y=friction, z=sphereCount, w=attractorCount
        // Mesh particle params (used only when meshFlags.x != 0; zero-default keeps billboard/ribbon emits unchanged)
        DirectX::XMFLOAT4 meshInitialScale = { 1.0f, 1.0f, 1.0f, 0.0f };        // xyz=scale, w=scaleRandomRange
        DirectX::XMFLOAT4 meshAngularAxisSpeed = { 0.0f, 1.0f, 0.0f, 0.0f };    // xyz=axis, w=rad/s
        DirectX::XMFLOAT4 meshAngularRandomOrient = { 0.0f, 0.0f, 0.0f, 0.0f }; // xyz=yaw/pitch/roll range, w=speed random
        DirectX::XMFLOAT4 meshFlags = { 0.0f, 0.0f, 0.0f, 0.0f };               // x=isMeshMode (0/1)
    };

    struct EffectParticleSortConstants
    {
        uint32_t increment = 0;
        uint32_t direction = 0;
        uint32_t pad[2] = {};
    };

    struct EffectParticleSceneConstants
    {
        DirectX::XMFLOAT4X4 viewProjection;
        DirectX::XMFLOAT4X4 inverseView;
        DirectX::XMFLOAT4 lightDirection = { 0.0f, -1.0f, 0.0f, 0.0f };
        DirectX::XMFLOAT4 lightColor = { 1.0f, 1.0f, 1.0f, 1.0f };
        DirectX::XMFLOAT4 cameraPosition = { 0.0f, 0.0f, 0.0f, 1.0f };
        DirectX::XMFLOAT4X4 lightViewProjection;
    };

    struct EffectParticleRenderConstants
    {
        uint32_t enableVelocityStretch = 1;
        float velocityStretchScale = 0.08f;
        float velocityStretchMaxAspect = 4.0f;
        float velocityStretchMinSpeed = 0.05f;
        float globalAlpha = 1.0f;
        float curlNoiseStrength = 0.0f;
        float curlNoiseScale = 0.1f;
        float curlMoveSpeed = 0.2f;
    };

    struct CoarseDepthConstants
    {
        DirectX::XMFLOAT4X4 viewMatrix;
        float nearClip = 0.1f;
        float farClip = 1000.0f;
        uint32_t aliveCount = 0;
        uint32_t pad = 0;
    };
    static_assert((sizeof(CoarseDepthConstants) % 16) == 0, "CoarseDepthConstants must stay 16-byte aligned");

    static_assert(sizeof(EffectParticleGpuHeader) == 16, "EffectParticleGpuHeader size mismatch");
    static_assert((sizeof(EffectParticleGpuData) % 16) == 0, "EffectParticleGpuData must stay 16-byte aligned");
    static_assert((sizeof(EffectParticleSimulationConstants) % 16) == 0, "EffectParticleSimulationConstants must stay 16-byte aligned");
    static_assert((sizeof(EffectParticleSortConstants) % 16) == 0, "EffectParticleSortConstants must stay 16-byte aligned");
    static_assert((sizeof(EffectParticleSceneConstants) % 16) == 0, "EffectParticleSceneConstants must stay 16-byte aligned");
    static_assert((sizeof(EffectParticleRenderConstants) % 16) == 0, "EffectParticleRenderConstants must stay 16-byte aligned");

    struct ParticleCounterSnapshot
    {
        // Must match GPU RWByteAddressBuffer layout in EffectParticleSoA.hlsli
        uint32_t aliveBillboard = 0;   // offset 0  = COUNTER_ALIVE_BILLBOARD
        uint32_t aliveMesh = 0;        // offset 4  = COUNTER_ALIVE_MESH
        uint32_t aliveRibbon = 0;      // offset 8  = COUNTER_ALIVE_RIBBON
        uint32_t aliveTotal = 0;       // offset 12 = COUNTER_ALIVE_TOTAL
        uint32_t allocatedPages = 0;   // offset 16 = COUNTER_ALLOCATED_PAGES
        uint32_t sparsePages = 0;      // offset 20 = COUNTER_SPARSE_PAGES
        uint32_t overflowCount = 0;    // offset 24 = COUNTER_OVERFLOW
        uint32_t droppedEmit = 0;      // offset 28 = COUNTER_DROPPED_EMIT
        uint32_t deadStackTop = 0;     // offset 32 = COUNTER_DEAD_STACK_TOP
    };

    struct ParticleCounterReadbackSlot
    {
        ComPtr<ID3D12Resource> resource;
        uint64_t submittedFrame = 0;
    };

    struct ParticlePageMetadata
    {
        uint32_t pageHandle = 0;
        ParticlePageState state = ParticlePageState::Free;
        uint32_t ownerEmitter = 0;
        uint32_t capacity = kEffectParticlePageSize;
        uint32_t liveCount = 0;
        uint32_t deadCount = kEffectParticlePageSize;
        uint32_t freeListHead = 0;
        uint32_t nextPageHandle = 0xFFFFFFFFu;
        uint32_t prevPageHandle = 0xFFFFFFFFu;
        uint32_t lastTouchedFrame = 0;
        uint32_t occupancyQ16 = 0;
        uint32_t flags = 0;
    };

    struct ParticleArenaAllocation
    {
        uint32_t basePage = 0;
        uint32_t pageCount = 0;
        uint32_t baseSlot = 0;
        uint32_t capacity = 0;
        EffectParticleDrawMode rendererType = EffectParticleDrawMode::Billboard;
        std::unique_ptr<IBuffer> counterBuffer;
        std::unique_ptr<IBuffer> indirectArgsBuffer;
        D3D12_RESOURCE_STATES counterState = D3D12_RESOURCE_STATE_COMMON;
        D3D12_RESOURCE_STATES indirectArgsState = D3D12_RESOURCE_STATE_COMMON;
        uint64_t lastSeenFrame = 0;
        uint64_t lastReadbackFrame = 0;
        bool initialized = false;
        float lastSimTime = 0.0f;
        float spawnAccumulator = 0.0f;
        uint32_t lastSeed = 0;
        bool burstConsumed = false;
        uint32_t lastEstimatedAlive = 0;
        ParticleCounterSnapshot lastCompletedCounters = {};
        std::array<ParticleCounterReadbackSlot, kEffectParticleCounterReadbackLatency> counterReadbacks = {};
    };

    // SoA GPU struct sizes (must match HLSL)
    constexpr uint32_t kBillboardHotStride = 32u;
    constexpr uint32_t kBillboardWarmStride = 16u;
    constexpr uint32_t kBillboardColdStride = 32u;
    constexpr uint32_t kBillboardHeaderStride = 8u;
    constexpr uint32_t kMeshAttribHotStride = 64u;

    struct ParticleSharedArenaBuffers
    {
        // SoA streams (replace single particleDataBuffer)
        std::unique_ptr<IBuffer> billboardHotBuffer;
        std::unique_ptr<IBuffer> billboardWarmBuffer;
        std::unique_ptr<IBuffer> billboardColdBuffer;
        std::unique_ptr<IBuffer> billboardHeaderBuffer;
        std::unique_ptr<IBuffer> meshAttribHotBuffer; // Mesh renderer bin only; billboards/ribbons ignore
        std::unique_ptr<IBuffer> ribbonHistoryBuffer;
        std::unique_ptr<IBuffer> deadListBuffer;
        // Per-frame scratch
        std::unique_ptr<IBuffer> aliveListBuffer;
        std::unique_ptr<IBuffer> pageAliveCountBuffer;
        std::unique_ptr<IBuffer> pageAliveOffsetBuffer;
        // Bin system (Phase 2)
        std::unique_ptr<IBuffer> binCounterBuffer;      // 4B * MAX_BINS
        std::unique_ptr<IBuffer> binIndexBuffer;         // 4B * totalCapacity (bin-sorted indices)
        std::unique_ptr<IBuffer> binOffsetBuffer;        // 4B * MAX_BINS (exclusive prefix per bin)
        std::unique_ptr<IBuffer> binIndirectArgsBuffer;  // 16B * MAX_BINS (D3D12_DRAW_ARGUMENTS per bin)
        // CoarseDepthBin (Phase 3)
        std::unique_ptr<IBuffer> depthBinCounterBuffer;    // 4B * 32
        std::unique_ptr<IBuffer> depthBinIndexBuffer;       // 4B * totalCapacity
        std::unique_ptr<IBuffer> depthBinIndirectArgsBuffer; // 16B * 32
        uint32_t totalPages = 0;
        uint32_t totalCapacity = 0;
        // Per-renderer budget tracking (allocated slot counts)
        uint32_t billboardAllocatedSlots = 0u;
        uint32_t meshAllocatedSlots = 0u;
        uint32_t ribbonAllocatedSlots = 0u;
        D3D12_RESOURCE_STATES billboardHotState = D3D12_RESOURCE_STATE_COMMON;
        D3D12_RESOURCE_STATES billboardWarmState = D3D12_RESOURCE_STATE_COMMON;
        D3D12_RESOURCE_STATES billboardColdState = D3D12_RESOURCE_STATE_COMMON;
        D3D12_RESOURCE_STATES billboardHeaderState = D3D12_RESOURCE_STATE_COMMON;
        D3D12_RESOURCE_STATES meshAttribHotState = D3D12_RESOURCE_STATE_COMMON;
        D3D12_RESOURCE_STATES ribbonHistoryState = D3D12_RESOURCE_STATE_COMMON;
        D3D12_RESOURCE_STATES deadListState = D3D12_RESOURCE_STATE_COMMON;
        D3D12_RESOURCE_STATES aliveListState = D3D12_RESOURCE_STATE_COMMON;
        D3D12_RESOURCE_STATES pageAliveCountState = D3D12_RESOURCE_STATE_COMMON;
        D3D12_RESOURCE_STATES pageAliveOffsetState = D3D12_RESOURCE_STATE_COMMON;
        D3D12_RESOURCE_STATES binCounterState = D3D12_RESOURCE_STATE_COMMON;
        D3D12_RESOURCE_STATES binIndexState = D3D12_RESOURCE_STATE_COMMON;
        D3D12_RESOURCE_STATES binOffsetState = D3D12_RESOURCE_STATE_COMMON;
        D3D12_RESOURCE_STATES binIndirectArgsState = D3D12_RESOURCE_STATE_COMMON;
        D3D12_RESOURCE_STATES depthBinCounterState = D3D12_RESOURCE_STATE_COMMON;
        D3D12_RESOURCE_STATES depthBinIndexState = D3D12_RESOURCE_STATE_COMMON;
        D3D12_RESOURCE_STATES depthBinIndirectArgsState = D3D12_RESOURCE_STATE_COMMON;
        std::vector<ParticlePageMetadata> pageTable;
    };

    struct BillboardDrawEntry
    {
        const EffectParticlePacket* packet = nullptr;
        D3D12_GPU_VIRTUAL_ADDRESS aliveListGpuVa = 0ull;
        D3D12_GPU_VIRTUAL_ADDRESS billboardHotGpuVa = 0ull;
        D3D12_GPU_VIRTUAL_ADDRESS billboardWarmGpuVa = 0ull;
        DX12Buffer* indirectArgsBuffer = nullptr;         // legacy per-emitter indirect args
        DX12Buffer* binIndirectArgsBuffer = nullptr;      // per-bin indirect args (Phase 2)
        D3D12_GPU_VIRTUAL_ADDRESS binIndexGpuVa = 0ull;   // bin-sorted particle indices
        DX12Buffer* depthBinIndirectArgsBuffer = nullptr;  // per-depth-bin indirect args (Phase 3)
        D3D12_GPU_VIRTUAL_ADDRESS depthBinIndexGpuVa = 0ull;
        EffectParticleBlendMode blendMode = EffectParticleBlendMode::PremultipliedAlpha;
    };

    struct MeshDrawEntry
    {
        const EffectParticlePacket* packet = nullptr;
        D3D12_GPU_VIRTUAL_ADDRESS aliveListGpuVa = 0ull;    // t0
        D3D12_GPU_VIRTUAL_ADDRESS hotGpuVa = 0ull;          // t1 (BillboardHot - position/velocity)
        D3D12_GPU_VIRTUAL_ADDRESS warmGpuVa = 0ull;         // t2 (BillboardWarm - packedColor)
        D3D12_GPU_VIRTUAL_ADDRESS headerGpuVa = 0ull;       // t3 (BillboardHeader - alive flag)
        D3D12_GPU_VIRTUAL_ADDRESS meshAttribHotGpuVa = 0ull;// t4 (MeshAttribHot - rotation/scale)
        DX12Buffer* indirectArgsBuffer = nullptr;
        uint32_t drawCount = 0; // CPU-side estimate; DrawIndexedInstanced uses this as instanceCount
    };

    struct RibbonDrawEntry
    {
        const EffectParticlePacket* packet = nullptr;
        D3D12_GPU_VIRTUAL_ADDRESS aliveListGpuVa = 0ull;
        D3D12_GPU_VIRTUAL_ADDRESS particleDataGpuVa = 0ull;
        D3D12_GPU_VIRTUAL_ADDRESS warmGpuVa = 0ull;
        D3D12_GPU_VIRTUAL_ADDRESS ribbonHistoryGpuVa = 0ull;
        DX12Buffer* indirectArgsBuffer = nullptr;
        EffectParticleBlendMode blendMode = EffectParticleBlendMode::PremultipliedAlpha;
    };

    struct EffectParticleDx12Resources
    {
        ComPtr<ID3D12RootSignature> simulationRootSignature;
        ComPtr<ID3D12PipelineState> initializePipelineState;
        ComPtr<ID3D12PipelineState> emitPipelineState;
        ComPtr<ID3D12PipelineState> updatePipelineState;
        ComPtr<ID3D12PipelineState> resetCountersPipelineState;
        ComPtr<ID3D12PipelineState> prefixSumPipelineState;
        ComPtr<ID3D12PipelineState> scatterAlivePipelineState;
        ComPtr<ID3D12PipelineState> buildDrawArgsPipelineState;
        ComPtr<ID3D12PipelineState> buildBinsPipelineState;
        ComPtr<ID3D12PipelineState> buildBinArgsPipelineState;
        ComPtr<ID3D12RootSignature> binRootSignature;      // for BuildBins + BuildBinArgs
        ComPtr<ID3D12CommandSignature> binCommandSignature; // per-bin ExecuteIndirect
        ComPtr<ID3D12PipelineState> coarseDepthPipelineState;
        ComPtr<ID3D12PipelineState> depthBinArgsPipelineState;
        ComPtr<ID3D12RootSignature> coarseDepthRootSignature;
        ComPtr<ID3D12CommandSignature> depthBinCommandSignature;
        ComPtr<ID3D12RootSignature> sortRootSignature;
        ComPtr<ID3D12PipelineState> sortB2PipelineState;
        ComPtr<ID3D12PipelineState> sortC2PipelineState;
        ComPtr<ID3D12RootSignature> billboardRootSignature;
        ComPtr<ID3D12PipelineState> billboardPipelineState;
        ComPtr<ID3D12PipelineState> billboardBlendPSOs[static_cast<int>(EffectParticleBlendMode::EnumCount)];
        ComPtr<ID3D12CommandSignature> billboardCommandSignature;
        ComPtr<ID3D12RootSignature> ribbonRootSignature;
        ComPtr<ID3D12PipelineState> ribbonPipelineState;
        ComPtr<ID3D12PipelineState> ribbonBlendPSOs[static_cast<int>(EffectParticleBlendMode::EnumCount)];
        ComPtr<ID3D12RootSignature> meshRootSignature;
        ComPtr<ID3D12PipelineState> meshPipelineState;
        ComPtr<ID3D12DescriptorHeap> textureHeap;
        D3D12_CPU_DESCRIPTOR_HANDLE textureHeapCpu = {};
        D3D12_GPU_DESCRIPTOR_HANDLE textureHeapGpu = {};
        UINT textureHeapDescriptorSize = 0;
        std::shared_ptr<ITexture> defaultTexture;
        std::unique_ptr<ITexture> curlNoiseTexture;
        ParticleSharedArenaBuffers sharedArena;
        std::unordered_map<uint32_t, ParticleArenaAllocation> runtimeAllocations;
        uint64_t frameCounter = 0;
        bool warnedNonDx12 = false;
        bool warnedMissingMesh = false;

        // Trail pipeline
        ComPtr<ID3D12RootSignature> trailRootSignature;
        ComPtr<ID3D12PipelineState> trailPipelineState;
    };

    EffectParticleDx12Resources& GetEffectParticleDx12Resources();
    uint32_t GetParticleSortPriority(EffectParticleSortMode mode);
    float ComputeDistanceSqToCamera(const EffectParticlePacket& packet, const RenderContext& rc);
    uint32_t AlignParticleCapacity(uint32_t requested);
    uint32_t ComputeParticlePageCount(uint32_t requested);
    void ResetEmitterSimulationState(ParticleArenaAllocation& runtimeBuffers);
    bool EnsureSharedArenaCapacity(EffectParticleDx12Resources& resources, IResourceFactory* factory, uint32_t requiredTotalPages);
    void ReleaseArenaPages(ParticleSharedArenaBuffers& sharedArena, const ParticleArenaAllocation& allocation);
    bool ReserveArenaPages(ParticleSharedArenaBuffers& sharedArena, uint32_t runtimeInstanceId, uint32_t pageCount, uint32_t& outBasePage);
    void TransitionBuffer(ID3D12GraphicsCommandList* commandList, ID3D12Resource* resource, D3D12_RESOURCE_STATES& currentState, D3D12_RESOURCE_STATES nextState);
    void AddUavBarrier(ID3D12GraphicsCommandList* commandList, ID3D12Resource* resource);
    bool EnsureCounterReadbackBuffers(DX12Device* device, ParticleArenaAllocation& runtimeBuffers);
    void ConsumeCounterReadback(ParticleArenaAllocation& runtimeBuffers, uint64_t currentFrame);
    void QueueCounterReadback(ID3D12GraphicsCommandList* commandList, ParticleArenaAllocation& runtimeBuffers, DX12Buffer* counterBuffer, uint64_t currentFrame);
    bool LoadBlob(const wchar_t* path, ComPtr<ID3DBlob>& outBlob);
    D3D12_BLEND_DESC CreateAdditiveBlendDesc();
    D3D12_RASTERIZER_DESC CreateRasterDesc(D3D12_CULL_MODE cullMode);
    D3D12_DEPTH_STENCIL_DESC CreateReadOnlyDepthDesc();
    bool CreateSimulationPipelines(DX12Device* device, EffectParticleDx12Resources& resources);
    bool CreateSortPipelines(DX12Device* device, EffectParticleDx12Resources& resources);
    bool CreateBillboardPipeline(DX12Device* device, EffectParticleDx12Resources& resources);
    bool CreateRibbonPipeline(DX12Device* device, EffectParticleDx12Resources& resources);
    bool CreateMeshPipeline(DX12Device* device, EffectParticleDx12Resources& resources);
    bool CreateTrailPipeline(DX12Device* device, EffectParticleDx12Resources& resources);
    bool CreateTextureDescriptorHeap(DX12Device* device, EffectParticleDx12Resources& resources);
    bool EnsureCurlNoiseTexture(DX12Device* device, EffectParticleDx12Resources& resources);
    bool EnsurePassResources(DX12Device* device, EffectParticleDx12Resources& resources);
    ParticleArenaAllocation* EnsureRuntimeAllocation(EffectParticleDx12Resources& resources, IResourceFactory* factory, uint32_t runtimeInstanceId, uint32_t maxParticles, EffectParticleDrawMode drawMode);
    DX12Texture* ResolveParticleTexture(EffectParticleDx12Resources& resources, const EffectParticlePacket& packet);
    bool DispatchBitonicSort(DX12CommandList* dx12CommandList, ID3D12GraphicsCommandList* nativeCommandList, EffectParticleDx12Resources& resources, DX12Buffer* headerBuffer, uint32_t particleCapacity);
    void FillSceneConstants(EffectParticleSceneConstants& sceneConstants, const RenderContext& rc);
    D3D12_CPU_DESCRIPTOR_HANDLE OffsetCpuHandle(D3D12_CPU_DESCRIPTOR_HANDLE handle, UINT descriptorSize, UINT offset);
    D3D12_GPU_DESCRIPTOR_HANDLE OffsetGpuHandle(D3D12_GPU_DESCRIPTOR_HANDLE handle, UINT descriptorSize, UINT offset);
    D3D12_GPU_VIRTUAL_ADDRESS OffsetGpuVirtualAddress(DX12Buffer* buffer, uint64_t byteOffset);

    EffectParticleDx12Resources& GetEffectParticleDx12Resources()
    {
        static EffectParticleDx12Resources resources;
        return resources;
    }

    uint32_t GetParticleSortPriority(EffectParticleSortMode mode)
    {
        switch (mode) {
        case EffectParticleSortMode::BackToFront:
            return 0;
        case EffectParticleSortMode::FrontToBack:
            return 2;
        case EffectParticleSortMode::None:
        default:
            return 1;
        }
    }

    float ComputeDistanceSqToCamera(const EffectParticlePacket& packet, const RenderContext& rc)
    {
        const float dx = packet.boundsCenter.x - rc.cameraPosition.x;
        const float dy = packet.boundsCenter.y - rc.cameraPosition.y;
        const float dz = packet.boundsCenter.z - rc.cameraPosition.z;
        return dx * dx + dy * dy + dz * dz;
    }

    // D3D12 dispatch limit: 65535 thread groups per dimension × 64 threads = 4,194,240 max.
    // Capacity beyond this requires 2D dispatch or shader-level offset (future work).
    constexpr uint32_t kMaxSingleDispatchParticles = 65535u * 64u;

    uint32_t AlignParticleCapacity(uint32_t requested)
    {
        return AlignEffectParticleCount(requested);
    }

    uint32_t ComputeParticlePageCount(uint32_t requested)
    {
        const uint32_t alignedCapacity = AlignParticleCapacity(requested);
        return (std::max)(1u, (alignedCapacity + kEffectParticlePageSize - 1u) / kEffectParticlePageSize);
    }

    void ResetEmitterSimulationState(ParticleArenaAllocation& runtimeBuffers)
    {
        runtimeBuffers.spawnAccumulator = 0.0f;
        runtimeBuffers.burstConsumed = false;
        runtimeBuffers.lastSimTime = 0.0f;
        runtimeBuffers.lastCompletedCounters.deadStackTop = runtimeBuffers.capacity;
        runtimeBuffers.lastCompletedCounters.aliveBillboard = 0u;
        runtimeBuffers.lastCompletedCounters.aliveMesh = 0u;
        runtimeBuffers.lastCompletedCounters.aliveRibbon = 0u;
        runtimeBuffers.lastCompletedCounters.aliveTotal = 0u;
        runtimeBuffers.lastCompletedCounters.overflowCount = 0u;
        runtimeBuffers.lastEstimatedAlive = 0;
        for (auto& slot : runtimeBuffers.counterReadbacks) {
            slot.submittedFrame = 0u;
        }
    }

    void ReleaseArenaPages(ParticleSharedArenaBuffers& sharedArena, const ParticleArenaAllocation& allocation)
    {
        if (allocation.pageCount == 0u || allocation.basePage >= sharedArena.pageTable.size()) {
            return;
        }

        const uint32_t endPage = (std::min)(allocation.basePage + allocation.pageCount, static_cast<uint32_t>(sharedArena.pageTable.size()));
        for (uint32_t pageIndex = allocation.basePage; pageIndex < endPage; ++pageIndex) {
            auto& page = sharedArena.pageTable[pageIndex];
            page.state = ParticlePageState::Free;
            page.ownerEmitter = 0;
            page.liveCount = 0;
            page.deadCount = page.capacity;
            page.freeListHead = 0;
            page.nextPageHandle = 0xFFFFFFFFu;
            page.prevPageHandle = 0xFFFFFFFFu;
            page.occupancyQ16 = 0;
            page.flags = 0;
        }
    }

    bool ReserveArenaPages(ParticleSharedArenaBuffers& sharedArena, uint32_t runtimeInstanceId, uint32_t pageCount, uint32_t& outBasePage)
    {
        if (pageCount == 0u || sharedArena.pageTable.size() < pageCount) {
            return false;
        }

        uint32_t runLength = 0u;
        uint32_t runStart = 0u;
        for (uint32_t pageIndex = 0; pageIndex < sharedArena.pageTable.size(); ++pageIndex) {
            if (sharedArena.pageTable[pageIndex].state == ParticlePageState::Free) {
                if (runLength == 0u) {
                    runStart = pageIndex;
                }
                runLength++;
                if (runLength >= pageCount) {
                    outBasePage = runStart;
                    const uint32_t endPage = runStart + pageCount;
                    for (uint32_t reserveIndex = runStart; reserveIndex < endPage; ++reserveIndex) {
                        auto& page = sharedArena.pageTable[reserveIndex];
                        page.state = ParticlePageState::Reserved;
                        page.ownerEmitter = runtimeInstanceId;
                        page.liveCount = 0;
                        page.deadCount = page.capacity;
                        page.freeListHead = 0;
                        page.prevPageHandle = (reserveIndex > runStart) ? (reserveIndex - 1u) : 0xFFFFFFFFu;
                        page.nextPageHandle = (reserveIndex + 1u < endPage) ? (reserveIndex + 1u) : 0xFFFFFFFFu;
                        page.occupancyQ16 = 0;
                        page.flags = 0;
                    }
                    return true;
                }
            } else {
                runLength = 0u;
            }
        }

        return false;
    }

    bool EnsureSharedArenaCapacity(EffectParticleDx12Resources& resources, IResourceFactory* factory, uint32_t requiredTotalPages)
    {
        auto& sharedArena = resources.sharedArena;
        if (!factory || requiredTotalPages == 0u) {
            return false;
        }

        const bool hasBuffers =
            sharedArena.billboardHotBuffer &&
            sharedArena.billboardWarmBuffer &&
            sharedArena.billboardColdBuffer &&
            sharedArena.billboardHeaderBuffer &&
            sharedArena.meshAttribHotBuffer &&
            sharedArena.ribbonHistoryBuffer &&
            sharedArena.deadListBuffer &&
            sharedArena.aliveListBuffer &&
            sharedArena.pageAliveCountBuffer &&
            sharedArena.pageAliveOffsetBuffer;

        if (hasBuffers && sharedArena.totalPages >= requiredTotalPages) {
            return true;
        }

        uint32_t targetPages = sharedArena.totalPages;
        if (targetPages == 0u) {
            targetPages = (std::max)(requiredTotalPages, kEffectParticleArenaGrowthPages);
        }
        while (targetPages < requiredTotalPages) {
            targetPages += (std::max)(kEffectParticleArenaGrowthPages, targetPages / 2u);
        }

        const uint32_t totalCapacity = targetPages * kEffectParticlePageSize;
        // SoA buffers: Hot(32B), Warm(16B), Cold(32B), Header(8B)
        auto newHotBuffer = factory->CreateBuffer(kBillboardHotStride * totalCapacity, BufferType::UAVStorage, nullptr);
        auto newWarmBuffer = factory->CreateBuffer(kBillboardWarmStride * totalCapacity, BufferType::UAVStorage, nullptr);
        auto newColdBuffer = factory->CreateBuffer(kBillboardColdStride * totalCapacity, BufferType::UAVStorage, nullptr);
        auto newHeaderBuffer = factory->CreateBuffer(kBillboardHeaderStride * totalCapacity, BufferType::UAVStorage, nullptr);
        // Mesh attribute hot stream (quaternion + scale + angular velocity). Allocated for every arena so
        // the u8 slot is always bindable — billboard/ribbon paths simply skip writing it via gMeshFlags.x.
        auto newMeshAttribHotBuffer = factory->CreateBuffer(kMeshAttribHotStride * totalCapacity, BufferType::UAVStorage, nullptr);
        auto newRibbonHistoryBuffer = factory->CreateBuffer(static_cast<uint32_t>(sizeof(DirectX::XMFLOAT4) * totalCapacity * kEffectParticleRibbonHistoryLength), BufferType::UAVStorage, nullptr);
        auto newDeadListBuffer = factory->CreateBuffer(static_cast<uint32_t>(sizeof(uint32_t) * totalCapacity), BufferType::UAVStorage, nullptr);
        // Per-frame scratch
        auto newAliveListBuffer = factory->CreateBuffer(static_cast<uint32_t>(sizeof(uint32_t) * totalCapacity), BufferType::UAVStorage, nullptr);
        auto newPageAliveCountBuffer = factory->CreateBuffer(static_cast<uint32_t>(sizeof(uint32_t) * targetPages), BufferType::UAVStorage, nullptr);
        auto newPageAliveOffsetBuffer = factory->CreateBuffer(static_cast<uint32_t>(sizeof(uint32_t) * targetPages), BufferType::UAVStorage, nullptr);
        // Bin system buffers
        constexpr uint32_t kMaxBins = 16u;
        auto newBinCounterBuffer = factory->CreateBuffer(static_cast<uint32_t>(sizeof(uint32_t) * kMaxBins), BufferType::UAVStorage, nullptr);
        auto newBinIndexBuffer = factory->CreateBuffer(static_cast<uint32_t>(sizeof(uint32_t) * totalCapacity), BufferType::UAVStorage, nullptr);
        auto newBinOffsetBuffer = factory->CreateBuffer(static_cast<uint32_t>(sizeof(uint32_t) * kMaxBins), BufferType::UAVStorage, nullptr);
        auto newBinIndirectArgsBuffer = factory->CreateBuffer(static_cast<uint32_t>(16u * kMaxBins), BufferType::UAVStorage, nullptr);
        // CoarseDepthBin buffers
        constexpr uint32_t kDepthBins = 32u;
        auto newDepthBinCounterBuffer = factory->CreateBuffer(static_cast<uint32_t>(sizeof(uint32_t) * kDepthBins), BufferType::UAVStorage, nullptr);
        auto newDepthBinIndexBuffer = factory->CreateBuffer(static_cast<uint32_t>(sizeof(uint32_t) * totalCapacity), BufferType::UAVStorage, nullptr);
        auto newDepthBinIndirectArgsBuffer = factory->CreateBuffer(static_cast<uint32_t>(16u * kDepthBins), BufferType::UAVStorage, nullptr);
        if (!newHotBuffer || !newWarmBuffer || !newColdBuffer || !newHeaderBuffer ||
            !newMeshAttribHotBuffer ||
            !newRibbonHistoryBuffer || !newDeadListBuffer ||
            !newAliveListBuffer || !newPageAliveCountBuffer || !newPageAliveOffsetBuffer ||
            !newBinCounterBuffer || !newBinIndexBuffer || !newBinOffsetBuffer || !newBinIndirectArgsBuffer ||
            !newDepthBinCounterBuffer || !newDepthBinIndexBuffer || !newDepthBinIndirectArgsBuffer) {
            return false;
        }

        sharedArena.billboardHotBuffer = std::move(newHotBuffer);
        sharedArena.billboardWarmBuffer = std::move(newWarmBuffer);
        sharedArena.billboardColdBuffer = std::move(newColdBuffer);
        sharedArena.billboardHeaderBuffer = std::move(newHeaderBuffer);
        sharedArena.meshAttribHotBuffer = std::move(newMeshAttribHotBuffer);
        sharedArena.ribbonHistoryBuffer = std::move(newRibbonHistoryBuffer);
        sharedArena.deadListBuffer = std::move(newDeadListBuffer);
        sharedArena.aliveListBuffer = std::move(newAliveListBuffer);
        sharedArena.pageAliveCountBuffer = std::move(newPageAliveCountBuffer);
        sharedArena.pageAliveOffsetBuffer = std::move(newPageAliveOffsetBuffer);
        sharedArena.binCounterBuffer = std::move(newBinCounterBuffer);
        sharedArena.binIndexBuffer = std::move(newBinIndexBuffer);
        sharedArena.binOffsetBuffer = std::move(newBinOffsetBuffer);
        sharedArena.binIndirectArgsBuffer = std::move(newBinIndirectArgsBuffer);
        sharedArena.depthBinCounterBuffer = std::move(newDepthBinCounterBuffer);
        sharedArena.depthBinIndexBuffer = std::move(newDepthBinIndexBuffer);
        sharedArena.depthBinIndirectArgsBuffer = std::move(newDepthBinIndirectArgsBuffer);
        sharedArena.totalPages = targetPages;
        sharedArena.totalCapacity = totalCapacity;
        sharedArena.billboardHotState = D3D12_RESOURCE_STATE_COMMON;
        sharedArena.billboardWarmState = D3D12_RESOURCE_STATE_COMMON;
        sharedArena.billboardColdState = D3D12_RESOURCE_STATE_COMMON;
        sharedArena.billboardHeaderState = D3D12_RESOURCE_STATE_COMMON;
        sharedArena.meshAttribHotState = D3D12_RESOURCE_STATE_COMMON;
        sharedArena.ribbonHistoryState = D3D12_RESOURCE_STATE_COMMON;
        sharedArena.deadListState = D3D12_RESOURCE_STATE_COMMON;
        sharedArena.aliveListState = D3D12_RESOURCE_STATE_COMMON;
        sharedArena.pageAliveCountState = D3D12_RESOURCE_STATE_COMMON;
        sharedArena.pageAliveOffsetState = D3D12_RESOURCE_STATE_COMMON;
        sharedArena.binCounterState = D3D12_RESOURCE_STATE_COMMON;
        sharedArena.binIndexState = D3D12_RESOURCE_STATE_COMMON;
        sharedArena.binOffsetState = D3D12_RESOURCE_STATE_COMMON;
        sharedArena.binIndirectArgsState = D3D12_RESOURCE_STATE_COMMON;
        sharedArena.depthBinCounterState = D3D12_RESOURCE_STATE_COMMON;
        sharedArena.depthBinIndexState = D3D12_RESOURCE_STATE_COMMON;
        sharedArena.depthBinIndirectArgsState = D3D12_RESOURCE_STATE_COMMON;
        sharedArena.pageTable.clear();
        sharedArena.pageTable.resize(targetPages);
        for (uint32_t pageIndex = 0; pageIndex < targetPages; ++pageIndex) {
            auto& page = sharedArena.pageTable[pageIndex];
            page.pageHandle = pageIndex;
            page.state = ParticlePageState::Free;
            page.ownerEmitter = 0;
            page.capacity = kEffectParticlePageSize;
            page.liveCount = 0;
            page.deadCount = kEffectParticlePageSize;
            page.freeListHead = 0;
            page.nextPageHandle = 0xFFFFFFFFu;
            page.prevPageHandle = 0xFFFFFFFFu;
            page.lastTouchedFrame = 0;
            page.occupancyQ16 = 0;
            page.flags = 0;
        }

        uint32_t cursorPage = 0u;
        for (auto& [runtimeInstanceId, allocation] : resources.runtimeAllocations) {
            if (allocation.pageCount == 0u && allocation.capacity == 0u) {
                continue;
            }
            const uint32_t allocationPages = ComputeParticlePageCount((std::max)(allocation.capacity, 1u));
            if (cursorPage + allocationPages > targetPages) {
                return false;
            }

            allocation.basePage = cursorPage;
            allocation.pageCount = allocationPages;
            allocation.baseSlot = cursorPage * kEffectParticlePageSize;
            allocation.capacity = allocationPages * kEffectParticlePageSize;
            allocation.initialized = false;
            ResetEmitterSimulationState(allocation);

            const uint32_t endPage = cursorPage + allocationPages;
            for (uint32_t pageIndex = cursorPage; pageIndex < endPage; ++pageIndex) {
                auto& page = sharedArena.pageTable[pageIndex];
                page.state = ParticlePageState::Reserved;
                page.ownerEmitter = runtimeInstanceId;
                page.prevPageHandle = (pageIndex > cursorPage) ? (pageIndex - 1u) : 0xFFFFFFFFu;
                page.nextPageHandle = (pageIndex + 1u < endPage) ? (pageIndex + 1u) : 0xFFFFFFFFu;
            }
            cursorPage = endPage;
        }

        return true;
    }

    void TransitionBuffer(
        ID3D12GraphicsCommandList* commandList,
        ID3D12Resource* resource,
        D3D12_RESOURCE_STATES& currentState,
        D3D12_RESOURCE_STATES nextState)
    {
        if (!commandList || !resource || currentState == nextState) {
            return;
        }

        D3D12_RESOURCE_BARRIER barrier = {};
        barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        barrier.Transition.pResource = resource;
        barrier.Transition.StateBefore = currentState;
        barrier.Transition.StateAfter = nextState;
        barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        commandList->ResourceBarrier(1, &barrier);
        currentState = nextState;
    }

    void AddUavBarrier(ID3D12GraphicsCommandList* commandList, ID3D12Resource* resource)
    {
        if (!commandList || !resource) {
            return;
        }

        D3D12_RESOURCE_BARRIER barrier = {};
        barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
        barrier.UAV.pResource = resource;
        commandList->ResourceBarrier(1, &barrier);
    }

    bool EnsureCounterReadbackBuffers(DX12Device* device, ParticleArenaAllocation& runtimeBuffers)
    {
        if (!device || !device->GetDevice()) {
            return false;
        }

        auto* d3dDevice = device->GetDevice();
        for (auto& slot : runtimeBuffers.counterReadbacks) {
            if (slot.resource) {
                continue;
            }

            D3D12_HEAP_PROPERTIES heapProps = {};
            heapProps.Type = D3D12_HEAP_TYPE_READBACK;
            heapProps.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
            heapProps.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
            heapProps.CreationNodeMask = 1;
            heapProps.VisibleNodeMask = 1;

            D3D12_RESOURCE_DESC desc = {};
            desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
            desc.Width = 256u;
            desc.Height = 1;
            desc.DepthOrArraySize = 1;
            desc.MipLevels = 1;
            desc.Format = DXGI_FORMAT_UNKNOWN;
            desc.SampleDesc.Count = 1;
            desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

            const HRESULT hr = d3dDevice->CreateCommittedResource(
                &heapProps,
                D3D12_HEAP_FLAG_NONE,
                &desc,
                D3D12_RESOURCE_STATE_COPY_DEST,
                nullptr,
                IID_PPV_ARGS(&slot.resource));
            if (FAILED(hr)) {
                LOG_ERROR("[EffectParticlePass] Failed to create particle counter readback buffer");
                return false;
            }
        }

        return true;
    }

    void ConsumeCounterReadback(ParticleArenaAllocation& runtimeBuffers, uint64_t currentFrame)
    {
        for (auto& slot : runtimeBuffers.counterReadbacks) {
            if (!slot.resource || slot.submittedFrame == 0u || currentFrame <= (slot.submittedFrame + 1u)) {
                continue;
            }

            void* mapped = nullptr;
            const D3D12_RANGE readRange = { 0, sizeof(ParticleCounterSnapshot) };
            if (FAILED(slot.resource->Map(0, &readRange, &mapped)) || !mapped) {
                continue;
            }

            std::memcpy(&runtimeBuffers.lastCompletedCounters, mapped, sizeof(ParticleCounterSnapshot));
            const D3D12_RANGE writeRange = { 0, 0 };
            slot.resource->Unmap(0, &writeRange);
            slot.submittedFrame = 0u;
        }
    }

    void QueueCounterReadback(ID3D12GraphicsCommandList* commandList, ParticleArenaAllocation& runtimeBuffers, DX12Buffer* counterBuffer, uint64_t currentFrame)
    {
        if (!commandList || !counterBuffer || !counterBuffer->GetNativeResource()) {
            return;
        }

        const uint32_t slotIndex = static_cast<uint32_t>(currentFrame % runtimeBuffers.counterReadbacks.size());
        auto& slot = runtimeBuffers.counterReadbacks[slotIndex];
        if (!slot.resource) {
            return;
        }

        commandList->CopyBufferRegion(
            slot.resource.Get(),
            0u,
            counterBuffer->GetNativeResource(),
            0u,
            sizeof(ParticleCounterSnapshot));
        slot.submittedFrame = currentFrame;
    }

    bool LoadBlob(const wchar_t* path, ComPtr<ID3DBlob>& outBlob)
    {
        outBlob.Reset();
        const HRESULT hr = D3DReadFileToBlob(path, &outBlob);
        if (FAILED(hr)) {
            LOG_ERROR("[EffectParticlePass] Failed to load shader blob: %ls", path);
            return false;
        }
        return true;
    }

    D3D12_BLEND_DESC CreateBlendDesc(EffectParticleBlendMode mode)
    {
        D3D12_BLEND_DESC blendDesc = {};
        blendDesc.AlphaToCoverageEnable = FALSE;
        blendDesc.IndependentBlendEnable = FALSE;
        auto& rt = blendDesc.RenderTarget[0];
        rt.BlendEnable = TRUE;
        rt.LogicOpEnable = FALSE;
        rt.BlendOp = D3D12_BLEND_OP_ADD;
        rt.BlendOpAlpha = D3D12_BLEND_OP_ADD;
        rt.LogicOp = D3D12_LOGIC_OP_NOOP;
        rt.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

        switch (mode) {
        case EffectParticleBlendMode::Additive:
            rt.SrcBlend = D3D12_BLEND_ONE;         rt.DestBlend = D3D12_BLEND_ONE;
            rt.SrcBlendAlpha = D3D12_BLEND_ONE;    rt.DestBlendAlpha = D3D12_BLEND_ONE;
            break;
        case EffectParticleBlendMode::AlphaBlend:
            rt.SrcBlend = D3D12_BLEND_SRC_ALPHA;   rt.DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
            rt.SrcBlendAlpha = D3D12_BLEND_ONE;    rt.DestBlendAlpha = D3D12_BLEND_INV_SRC_ALPHA;
            break;
        case EffectParticleBlendMode::Multiply:
            rt.SrcBlend = D3D12_BLEND_DEST_COLOR;  rt.DestBlend = D3D12_BLEND_ZERO;
            rt.SrcBlendAlpha = D3D12_BLEND_ONE;    rt.DestBlendAlpha = D3D12_BLEND_ZERO;
            break;
        case EffectParticleBlendMode::SoftAdditive:
            rt.SrcBlend = D3D12_BLEND_ONE;         rt.DestBlend = D3D12_BLEND_INV_SRC_COLOR;
            rt.SrcBlendAlpha = D3D12_BLEND_ONE;    rt.DestBlendAlpha = D3D12_BLEND_ONE;
            break;
        default: // PremultipliedAlpha
            rt.SrcBlend = D3D12_BLEND_ONE;         rt.DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
            rt.SrcBlendAlpha = D3D12_BLEND_ONE;    rt.DestBlendAlpha = D3D12_BLEND_INV_SRC_ALPHA;
            break;
        }
        return blendDesc;
    }

    D3D12_BLEND_DESC CreateAdditiveBlendDesc() { return CreateBlendDesc(EffectParticleBlendMode::Additive); }
    D3D12_BLEND_DESC CreatePremultipliedAlphaBlendDesc() { return CreateBlendDesc(EffectParticleBlendMode::PremultipliedAlpha); }

    D3D12_RASTERIZER_DESC CreateRasterDesc(D3D12_CULL_MODE cullMode)
    {
        D3D12_RASTERIZER_DESC rasterDesc = {};
        rasterDesc.FillMode = D3D12_FILL_MODE_SOLID;
        rasterDesc.CullMode = cullMode;
        rasterDesc.FrontCounterClockwise = FALSE;
        rasterDesc.DepthBias = D3D12_DEFAULT_DEPTH_BIAS;
        rasterDesc.DepthBiasClamp = D3D12_DEFAULT_DEPTH_BIAS_CLAMP;
        rasterDesc.SlopeScaledDepthBias = D3D12_DEFAULT_SLOPE_SCALED_DEPTH_BIAS;
        rasterDesc.DepthClipEnable = TRUE;
        rasterDesc.MultisampleEnable = FALSE;
        rasterDesc.AntialiasedLineEnable = FALSE;
        rasterDesc.ForcedSampleCount = 0;
        rasterDesc.ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;
        return rasterDesc;
    }

    D3D12_DEPTH_STENCIL_DESC CreateReadOnlyDepthDesc()
    {
        D3D12_DEPTH_STENCIL_DESC depthDesc = {};
        depthDesc.DepthEnable = TRUE;
        depthDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
        depthDesc.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
        depthDesc.StencilEnable = FALSE;
        return depthDesc;
    }
}

namespace
{
    D3D12_CPU_DESCRIPTOR_HANDLE OffsetCpuHandle(D3D12_CPU_DESCRIPTOR_HANDLE handle, UINT descriptorSize, UINT offset)
    {
        handle.ptr += static_cast<SIZE_T>(descriptorSize) * static_cast<SIZE_T>(offset);
        return handle;
    }

    D3D12_GPU_DESCRIPTOR_HANDLE OffsetGpuHandle(D3D12_GPU_DESCRIPTOR_HANDLE handle, UINT descriptorSize, UINT offset)
    {
        handle.ptr += static_cast<UINT64>(descriptorSize) * static_cast<UINT64>(offset);
        return handle;
    }

    D3D12_GPU_VIRTUAL_ADDRESS OffsetGpuVirtualAddress(DX12Buffer* buffer, uint64_t byteOffset)
    {
        return buffer ? (buffer->GetGPUVirtualAddress() + byteOffset) : 0ull;
    }

    std::unique_ptr<ITexture> CreateCurlNoiseVolumeTexture(DX12Device* device)
    {
        if (!device) {
            return nullptr;
        }

        constexpr uint32_t kWidth = 32u;
        constexpr uint32_t kHeight = 32u;
        constexpr uint32_t kDepth = 32u;
        constexpr float kFrequency = 0.085f;
        constexpr int kSeed = 1337;
        constexpr float kOffsetY = 1000.0f;
        constexpr float kOffsetZ = 2000.0f;
        constexpr float kDerivativeStep = 1.0f;

        FastNoiseLite noise;
        noise.SetNoiseType(FastNoiseLite::NoiseType_OpenSimplex2);
        noise.SetFrequency(kFrequency);
        noise.SetSeed(kSeed);
        noise.SetFractalType(FastNoiseLite::FractalType_FBm);
        noise.SetFractalOctaves(3);

        std::vector<float> volumeData(static_cast<size_t>(kWidth) * static_cast<size_t>(kHeight) * static_cast<size_t>(kDepth) * 4u, 0.0f);

        auto samplePotential = [&](float x, float y, float z)
        {
            return DirectX::XMFLOAT3(
                noise.GetNoise(x, y, z),
                noise.GetNoise(x, y + kOffsetY, z),
                noise.GetNoise(x, y, z + kOffsetZ));
        };

        const float derivativeScale = 1.0f / (2.0f * kDerivativeStep);
        for (uint32_t z = 0; z < kDepth; ++z) {
            for (uint32_t y = 0; y < kHeight; ++y) {
                for (uint32_t x = 0; x < kWidth; ++x) {
                    const float fx = static_cast<float>(x);
                    const float fy = static_cast<float>(y);
                    const float fz = static_cast<float>(z);

                    const DirectX::XMFLOAT3 pY0 = samplePotential(fx, fy - kDerivativeStep, fz);
                    const DirectX::XMFLOAT3 pY1 = samplePotential(fx, fy + kDerivativeStep, fz);
                    const DirectX::XMFLOAT3 pZ0 = samplePotential(fx, fy, fz - kDerivativeStep);
                    const DirectX::XMFLOAT3 pZ1 = samplePotential(fx, fy, fz + kDerivativeStep);
                    const DirectX::XMFLOAT3 pX0 = samplePotential(fx - kDerivativeStep, fy, fz);
                    const DirectX::XMFLOAT3 pX1 = samplePotential(fx + kDerivativeStep, fy, fz);

                    const float vx = (pY1.z - pY0.z) * derivativeScale - (pZ1.y - pZ0.y) * derivativeScale;
                    const float vy = (pZ1.x - pZ0.x) * derivativeScale - (pX1.z - pX0.z) * derivativeScale;
                    const float vz = (pX1.y - pX0.y) * derivativeScale - (pY1.x - pY0.x) * derivativeScale;

                    const size_t index = ((static_cast<size_t>(z) * kHeight + static_cast<size_t>(y)) * kWidth + static_cast<size_t>(x)) * 4u;
                    volumeData[index + 0] = vx;
                    volumeData[index + 1] = vy;
                    volumeData[index + 2] = vz;
                    volumeData[index + 3] = 1.0f;
                }
            }
        }

        auto* d3dDevice = device->GetDevice();
        auto* cmdQueue = device->GetCommandQueue();
        if (!d3dDevice || !cmdQueue) {
            return nullptr;
        }

        D3D12_RESOURCE_DESC texDesc = {};
        texDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE3D;
        texDesc.Alignment = 0;
        texDesc.Width = kWidth;
        texDesc.Height = kHeight;
        texDesc.DepthOrArraySize = static_cast<UINT16>(kDepth);
        texDesc.MipLevels = 1;
        texDesc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
        texDesc.SampleDesc.Count = 1;
        texDesc.SampleDesc.Quality = 0;
        texDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
        texDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

        D3D12_HEAP_PROPERTIES defaultHeap = {};
        defaultHeap.Type = D3D12_HEAP_TYPE_DEFAULT;

        ComPtr<ID3D12Resource> textureResource;
        HRESULT hr = d3dDevice->CreateCommittedResource(
            &defaultHeap,
            D3D12_HEAP_FLAG_NONE,
            &texDesc,
            D3D12_RESOURCE_STATE_COPY_DEST,
            nullptr,
            IID_PPV_ARGS(&textureResource));
        if (FAILED(hr)) {
            LOG_ERROR("[EffectParticlePass] Failed to create curl noise volume texture");
            return nullptr;
        }

        D3D12_PLACED_SUBRESOURCE_FOOTPRINT layout = {};
        UINT numRows = 0;
        UINT64 rowSizeInBytes = 0;
        UINT64 totalBytes = 0;
        d3dDevice->GetCopyableFootprints(&texDesc, 0, 1, 0, &layout, &numRows, &rowSizeInBytes, &totalBytes);

        D3D12_HEAP_PROPERTIES uploadHeap = {};
        uploadHeap.Type = D3D12_HEAP_TYPE_UPLOAD;

        D3D12_RESOURCE_DESC uploadDesc = {};
        uploadDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
        uploadDesc.Width = totalBytes;
        uploadDesc.Height = 1;
        uploadDesc.DepthOrArraySize = 1;
        uploadDesc.MipLevels = 1;
        uploadDesc.Format = DXGI_FORMAT_UNKNOWN;
        uploadDesc.SampleDesc.Count = 1;
        uploadDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

        ComPtr<ID3D12Resource> uploadBuffer;
        hr = d3dDevice->CreateCommittedResource(
            &uploadHeap,
            D3D12_HEAP_FLAG_NONE,
            &uploadDesc,
            D3D12_RESOURCE_STATE_GENERIC_READ,
            nullptr,
            IID_PPV_ARGS(&uploadBuffer));
        if (FAILED(hr)) {
            LOG_ERROR("[EffectParticlePass] Failed to create curl noise upload buffer");
            return nullptr;
        }

        uint8_t* mappedData = nullptr;
        hr = uploadBuffer->Map(0, nullptr, reinterpret_cast<void**>(&mappedData));
        if (FAILED(hr) || !mappedData) {
            LOG_ERROR("[EffectParticlePass] Failed to map curl noise upload buffer");
            return nullptr;
        }

        const size_t srcRowPitch = static_cast<size_t>(kWidth) * sizeof(float) * 4u;
        const size_t srcSlicePitch = srcRowPitch * static_cast<size_t>(kHeight);
        const size_t dstSlicePitch = static_cast<size_t>(layout.Footprint.RowPitch) * static_cast<size_t>(numRows);
        uint8_t* dstBase = mappedData + layout.Offset;
        const uint8_t* srcBase = reinterpret_cast<const uint8_t*>(volumeData.data());
        for (uint32_t z = 0; z < kDepth; ++z) {
            uint8_t* dstSlice = dstBase + static_cast<size_t>(z) * dstSlicePitch;
            const uint8_t* srcSlice = srcBase + static_cast<size_t>(z) * srcSlicePitch;
            for (uint32_t row = 0; row < kHeight; ++row) {
                std::memcpy(
                    dstSlice + static_cast<size_t>(row) * layout.Footprint.RowPitch,
                    srcSlice + static_cast<size_t>(row) * srcRowPitch,
                    srcRowPitch);
            }
        }
        uploadBuffer->Unmap(0, nullptr);

        ComPtr<ID3D12CommandAllocator> cmdAllocator;
        hr = d3dDevice->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&cmdAllocator));
        if (FAILED(hr)) {
            return nullptr;
        }

        ComPtr<ID3D12GraphicsCommandList> cmdList;
        hr = d3dDevice->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, cmdAllocator.Get(), nullptr, IID_PPV_ARGS(&cmdList));
        if (FAILED(hr)) {
            return nullptr;
        }

        D3D12_TEXTURE_COPY_LOCATION dst = {};
        dst.pResource = textureResource.Get();
        dst.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
        dst.SubresourceIndex = 0;

        D3D12_TEXTURE_COPY_LOCATION src = {};
        src.pResource = uploadBuffer.Get();
        src.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
        src.PlacedFootprint = layout;
        cmdList->CopyTextureRegion(&dst, 0, 0, 0, &src, nullptr);

        D3D12_RESOURCE_BARRIER barrier = {};
        barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        barrier.Transition.pResource = textureResource.Get();
        barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
        barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
        barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        cmdList->ResourceBarrier(1, &barrier);
        cmdList->Close();

        ID3D12CommandList* lists[] = { cmdList.Get() };
        cmdQueue->ExecuteCommandLists(1, lists);

        ComPtr<ID3D12Fence> fence;
        hr = d3dDevice->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence));
        if (FAILED(hr)) {
            return nullptr;
        }

        HANDLE fenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
        cmdQueue->Signal(fence.Get(), 1);
        if (fence->GetCompletedValue() < 1) {
            fence->SetEventOnCompletion(1, fenceEvent);
            WaitForSingleObject(fenceEvent, INFINITE);
        }
        CloseHandle(fenceEvent);

        return std::make_unique<DX12Texture>(
            device,
            textureResource,
            kWidth,
            kHeight,
            DXGI_FORMAT_R32G32B32A32_FLOAT,
            false);
    }

    bool CreateSimulationPipelines(DX12Device* device, EffectParticleDx12Resources& resources)
    {
        if (!device ||
            (resources.initializePipelineState &&
             resources.emitPipelineState &&
             resources.updatePipelineState &&
             resources.resetCountersPipelineState &&
             resources.prefixSumPipelineState &&
             resources.scatterAlivePipelineState &&
             resources.buildDrawArgsPipelineState)) {
            return resources.initializePipelineState != nullptr &&
                resources.emitPipelineState != nullptr &&
                resources.updatePipelineState != nullptr &&
                resources.resetCountersPipelineState != nullptr &&
                resources.prefixSumPipelineState != nullptr &&
                resources.scatterAlivePipelineState != nullptr &&
                resources.buildDrawArgsPipelineState != nullptr;
        }

        auto* d3dDevice = device->GetDevice();
        if (!d3dDevice) {
            return false;
        }

        D3D12_DESCRIPTOR_RANGE1 curlNoiseRange = {};
        curlNoiseRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
        curlNoiseRange.NumDescriptors = 1;
        curlNoiseRange.BaseShaderRegister = 1;
        curlNoiseRange.RegisterSpace = 0;
        curlNoiseRange.Flags = D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC_WHILE_SET_AT_EXECUTE;
        curlNoiseRange.OffsetInDescriptorsFromTableStart = 0;

        D3D12_DESCRIPTOR_RANGE1 srvRange = {};
        srvRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
        srvRange.NumDescriptors = 1;
        srvRange.BaseShaderRegister = 1;  // t1 for Cold (read-only in Update)
        srvRange.RegisterSpace = 0;
        srvRange.Flags = D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC_WHILE_SET_AT_EXECUTE;
        srvRange.OffsetInDescriptorsFromTableStart = 0;

        // Root params: b0, t0, u0-u8, descriptorTable(CurlNoise t1)
        // [0] b0  = CBV (simulation params)
        // [1] t0  = SRV (alive list prev / page table)
        // [2] u0  = UAV (BillboardHot)
        // [3] u1  = UAV (BillboardWarm)
        // [4] u2  = UAV (BillboardCold)
        // [5] u3  = UAV (BillboardHeader)
        // [6] u4  = UAV (DeadStack)
        // [7] u5  = UAV (Counter)
        // [8] u6  = UAV (RibbonHistory)
        // [9] u7  = UAV (PageAliveCount) -- only used by Update
        // [10] u8 = UAV (MeshAttribHot) -- written only when gMeshFlags.x != 0; bound always to satisfy validation
        // [11] descriptorTable = curl noise SRV
        D3D12_ROOT_PARAMETER1 params[12] = {};
        params[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
        params[0].Descriptor = { 0, 0, D3D12_ROOT_DESCRIPTOR_FLAG_DATA_STATIC_WHILE_SET_AT_EXECUTE };
        params[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
        params[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_SRV;
        params[1].Descriptor = { 0, 0, D3D12_ROOT_DESCRIPTOR_FLAG_DATA_STATIC_WHILE_SET_AT_EXECUTE };
        params[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
        params[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_UAV;
        params[2].Descriptor = { 0, 0, D3D12_ROOT_DESCRIPTOR_FLAG_DATA_VOLATILE };
        params[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
        params[3].ParameterType = D3D12_ROOT_PARAMETER_TYPE_UAV;
        params[3].Descriptor = { 1, 0, D3D12_ROOT_DESCRIPTOR_FLAG_DATA_VOLATILE };
        params[3].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
        params[4].ParameterType = D3D12_ROOT_PARAMETER_TYPE_UAV;
        params[4].Descriptor = { 2, 0, D3D12_ROOT_DESCRIPTOR_FLAG_DATA_VOLATILE };
        params[4].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
        params[5].ParameterType = D3D12_ROOT_PARAMETER_TYPE_UAV;
        params[5].Descriptor = { 3, 0, D3D12_ROOT_DESCRIPTOR_FLAG_DATA_VOLATILE };
        params[5].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
        params[6].ParameterType = D3D12_ROOT_PARAMETER_TYPE_UAV;
        params[6].Descriptor = { 4, 0, D3D12_ROOT_DESCRIPTOR_FLAG_DATA_VOLATILE };
        params[6].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
        params[7].ParameterType = D3D12_ROOT_PARAMETER_TYPE_UAV;
        params[7].Descriptor = { 5, 0, D3D12_ROOT_DESCRIPTOR_FLAG_DATA_VOLATILE };
        params[7].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
        params[8].ParameterType = D3D12_ROOT_PARAMETER_TYPE_UAV;
        params[8].Descriptor = { 6, 0, D3D12_ROOT_DESCRIPTOR_FLAG_DATA_VOLATILE };
        params[8].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
        params[9].ParameterType = D3D12_ROOT_PARAMETER_TYPE_UAV;
        params[9].Descriptor = { 7, 0, D3D12_ROOT_DESCRIPTOR_FLAG_DATA_VOLATILE };
        params[9].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
        params[10].ParameterType = D3D12_ROOT_PARAMETER_TYPE_UAV;
        params[10].Descriptor = { 8, 0, D3D12_ROOT_DESCRIPTOR_FLAG_DATA_VOLATILE };
        params[10].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
        params[11].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
        params[11].DescriptorTable.NumDescriptorRanges = 1;
        params[11].DescriptorTable.pDescriptorRanges = &curlNoiseRange;
        params[11].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

        D3D12_STATIC_SAMPLER_DESC curlSampler = {};
        curlSampler.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
        curlSampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
        curlSampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
        curlSampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
        curlSampler.ShaderRegister = 0;
        curlSampler.RegisterSpace = 0;
        curlSampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
        curlSampler.ComparisonFunc = D3D12_COMPARISON_FUNC_ALWAYS;
        curlSampler.MaxLOD = D3D12_FLOAT32_MAX;

        D3D12_VERSIONED_ROOT_SIGNATURE_DESC rootDesc = {};
        rootDesc.Version = D3D_ROOT_SIGNATURE_VERSION_1_1;
        rootDesc.Desc_1_1.NumParameters = _countof(params);
        rootDesc.Desc_1_1.pParameters = params;
        rootDesc.Desc_1_1.NumStaticSamplers = 1;
        rootDesc.Desc_1_1.pStaticSamplers = &curlSampler;
        rootDesc.Desc_1_1.Flags = D3D12_ROOT_SIGNATURE_FLAG_NONE;

        ComPtr<ID3DBlob> signatureBlob;
        ComPtr<ID3DBlob> errorBlob;
        HRESULT hr = D3D12SerializeVersionedRootSignature(&rootDesc, &signatureBlob, &errorBlob);
        if (FAILED(hr)) {
            if (errorBlob) {
                LOG_ERROR("[EffectParticlePass] Compute root signature serialize failed: %s",
                    static_cast<const char*>(errorBlob->GetBufferPointer()));
            }
            return false;
        }

        hr = d3dDevice->CreateRootSignature(
            0,
            signatureBlob->GetBufferPointer(),
            signatureBlob->GetBufferSize(),
            IID_PPV_ARGS(&resources.simulationRootSignature));
        if (FAILED(hr)) {
            LOG_ERROR("[EffectParticlePass] Failed to create simulation root signature");
            return false;
        }

        ComPtr<ID3DBlob> initializeBlob;
        ComPtr<ID3DBlob> emitBlob;
        ComPtr<ID3DBlob> updateBlob;
        ComPtr<ID3DBlob> resetBlob;
        ComPtr<ID3DBlob> prefixSumBlob;
        ComPtr<ID3DBlob> scatterAliveBlob;
        ComPtr<ID3DBlob> buildDrawArgsBlob;
        if (!LoadBlob(L"Data/Shader/EffectParticleInitialize_cs.cso", initializeBlob) ||
            !LoadBlob(L"Data/Shader/EffectParticleEmit_cs.cso", emitBlob) ||
            !LoadBlob(L"Data/Shader/EffectParticleUpdate_cs.cso", updateBlob) ||
            !LoadBlob(L"Data/Shader/EffectParticleResetCounters_cs.cso", resetBlob) ||
            !LoadBlob(L"Data/Shader/EffectParticlePrefixSum_cs.cso", prefixSumBlob) ||
            !LoadBlob(L"Data/Shader/EffectParticleScatterAlive_cs.cso", scatterAliveBlob) ||
            !LoadBlob(L"Data/Shader/EffectParticleBuildDrawArgs_cs.cso", buildDrawArgsBlob)) {
            return false;
        }

        auto createPso = [&](ID3DBlob* blob, ComPtr<ID3D12PipelineState>& outState, const char* label) -> bool
        {
            D3D12_COMPUTE_PIPELINE_STATE_DESC psoDesc = {};
            psoDesc.pRootSignature = resources.simulationRootSignature.Get();
            psoDesc.CS = { blob->GetBufferPointer(), blob->GetBufferSize() };
            const HRESULT psoHr = d3dDevice->CreateComputePipelineState(&psoDesc, IID_PPV_ARGS(&outState));
            if (FAILED(psoHr)) {
                LOG_ERROR("[EffectParticlePass] Failed to create %s pipeline state", label);
                return false;
            }
            return true;
        };

        return createPso(initializeBlob.Get(), resources.initializePipelineState, "particle initialize") &&
            createPso(emitBlob.Get(), resources.emitPipelineState, "particle emit") &&
            createPso(updateBlob.Get(), resources.updatePipelineState, "particle update") &&
            createPso(resetBlob.Get(), resources.resetCountersPipelineState, "particle reset-counters") &&
            createPso(prefixSumBlob.Get(), resources.prefixSumPipelineState, "particle prefix-sum") &&
            createPso(scatterAliveBlob.Get(), resources.scatterAlivePipelineState, "particle scatter-alive") &&
            createPso(buildDrawArgsBlob.Get(), resources.buildDrawArgsPipelineState, "particle build-draw-args");
    }

    bool CreateBinPipelines(DX12Device* device, EffectParticleDx12Resources& resources)
    {
        if (!device ||
            (resources.buildBinsPipelineState && resources.buildBinArgsPipelineState && resources.binRootSignature && resources.binCommandSignature)) {
            return resources.buildBinsPipelineState != nullptr;
        }

        auto* d3dDevice = device->GetDevice();
        if (!d3dDevice) {
            return false;
        }

        // Bin root signature:
        // [0] SRV t0 = AliveList
        // [1] SRV t1 = BillboardWarm
        // [2] SRV t2 = BillboardHeader
        // [3] UAV u0 = BinIndex
        // [4] UAV u1 = BinCounter
        // [5] UAV u2 = CounterBuffer (for aliveCount)
        D3D12_ROOT_PARAMETER1 binParams[6] = {};
        binParams[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_SRV;
        binParams[0].Descriptor = { 0, 0, D3D12_ROOT_DESCRIPTOR_FLAG_DATA_STATIC_WHILE_SET_AT_EXECUTE };
        binParams[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
        binParams[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_SRV;
        binParams[1].Descriptor = { 1, 0, D3D12_ROOT_DESCRIPTOR_FLAG_DATA_STATIC_WHILE_SET_AT_EXECUTE };
        binParams[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
        binParams[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_SRV;
        binParams[2].Descriptor = { 2, 0, D3D12_ROOT_DESCRIPTOR_FLAG_DATA_STATIC_WHILE_SET_AT_EXECUTE };
        binParams[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
        binParams[3].ParameterType = D3D12_ROOT_PARAMETER_TYPE_UAV;
        binParams[3].Descriptor = { 0, 0, D3D12_ROOT_DESCRIPTOR_FLAG_DATA_VOLATILE };
        binParams[3].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
        binParams[4].ParameterType = D3D12_ROOT_PARAMETER_TYPE_UAV;
        binParams[4].Descriptor = { 1, 0, D3D12_ROOT_DESCRIPTOR_FLAG_DATA_VOLATILE };
        binParams[4].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
        binParams[5].ParameterType = D3D12_ROOT_PARAMETER_TYPE_UAV;
        binParams[5].Descriptor = { 2, 0, D3D12_ROOT_DESCRIPTOR_FLAG_DATA_VOLATILE };
        binParams[5].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

        D3D12_VERSIONED_ROOT_SIGNATURE_DESC binRootDesc = {};
        binRootDesc.Version = D3D_ROOT_SIGNATURE_VERSION_1_1;
        binRootDesc.Desc_1_1.NumParameters = _countof(binParams);
        binRootDesc.Desc_1_1.pParameters = binParams;
        binRootDesc.Desc_1_1.Flags = D3D12_ROOT_SIGNATURE_FLAG_NONE;

        ComPtr<ID3DBlob> binSigBlob;
        ComPtr<ID3DBlob> binErrBlob;
        HRESULT hr = D3D12SerializeVersionedRootSignature(&binRootDesc, &binSigBlob, &binErrBlob);
        if (FAILED(hr)) {
            if (binErrBlob) {
                LOG_ERROR("[EffectParticlePass] Bin root signature serialize failed: %s",
                    static_cast<const char*>(binErrBlob->GetBufferPointer()));
            }
            return false;
        }

        hr = d3dDevice->CreateRootSignature(0, binSigBlob->GetBufferPointer(), binSigBlob->GetBufferSize(),
            IID_PPV_ARGS(&resources.binRootSignature));
        if (FAILED(hr)) {
            LOG_ERROR("[EffectParticlePass] Failed to create bin root signature");
            return false;
        }

        ComPtr<ID3DBlob> buildBinsBlob;
        ComPtr<ID3DBlob> buildBinArgsBlob;
        if (!LoadBlob(L"Data/Shader/EffectParticleBuildBins_cs.cso", buildBinsBlob) ||
            !LoadBlob(L"Data/Shader/EffectParticleBuildBinArgs_cs.cso", buildBinArgsBlob)) {
            return false;
        }

        auto createBinPso = [&](ID3DBlob* blob, ComPtr<ID3D12PipelineState>& outState, const char* label) -> bool
        {
            D3D12_COMPUTE_PIPELINE_STATE_DESC psoDesc = {};
            psoDesc.pRootSignature = resources.binRootSignature.Get();
            psoDesc.CS = { blob->GetBufferPointer(), blob->GetBufferSize() };
            const HRESULT psoHr = d3dDevice->CreateComputePipelineState(&psoDesc, IID_PPV_ARGS(&outState));
            if (FAILED(psoHr)) {
                LOG_ERROR("[EffectParticlePass] Failed to create %s pipeline state", label);
                return false;
            }
            return true;
        };

        if (!createBinPso(buildBinsBlob.Get(), resources.buildBinsPipelineState, "build-bins") ||
            !createBinPso(buildBinArgsBlob.Get(), resources.buildBinArgsPipelineState, "build-bin-args")) {
            return false;
        }

        // Command signature for per-bin ExecuteIndirect (D3D12_DRAW_ARGUMENTS = 16 bytes)
        D3D12_INDIRECT_ARGUMENT_DESC indirectArg = {};
        indirectArg.Type = D3D12_INDIRECT_ARGUMENT_TYPE_DRAW;

        D3D12_COMMAND_SIGNATURE_DESC cmdSigDesc = {};
        cmdSigDesc.ByteStride = 16u; // sizeof(D3D12_DRAW_ARGUMENTS)
        cmdSigDesc.NumArgumentDescs = 1;
        cmdSigDesc.pArgumentDescs = &indirectArg;

        hr = d3dDevice->CreateCommandSignature(&cmdSigDesc, nullptr, IID_PPV_ARGS(&resources.binCommandSignature));
        if (FAILED(hr)) {
            LOG_ERROR("[EffectParticlePass] Failed to create bin command signature");
            return false;
        }

        return true;
    }

    bool CreateCoarseDepthPipelines(DX12Device* device, EffectParticleDx12Resources& resources)
    {
        if (!device ||
            (resources.coarseDepthPipelineState && resources.depthBinArgsPipelineState && resources.coarseDepthRootSignature)) {
            return resources.coarseDepthPipelineState != nullptr;
        }

        auto* d3dDevice = device->GetDevice();
        if (!d3dDevice) {
            return false;
        }

        // CoarseDepth root signature:
        // [0] CBV b0, [1] SRV t0, [2] SRV t1, [3] UAV u0, [4] UAV u1, [5] UAV u2
        D3D12_ROOT_PARAMETER1 depthParams[6] = {};
        depthParams[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
        depthParams[0].Descriptor = { 0, 0, D3D12_ROOT_DESCRIPTOR_FLAG_DATA_STATIC_WHILE_SET_AT_EXECUTE };
        depthParams[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
        depthParams[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_SRV;
        depthParams[1].Descriptor = { 0, 0, D3D12_ROOT_DESCRIPTOR_FLAG_DATA_STATIC_WHILE_SET_AT_EXECUTE };
        depthParams[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
        depthParams[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_SRV;
        depthParams[2].Descriptor = { 1, 0, D3D12_ROOT_DESCRIPTOR_FLAG_DATA_STATIC_WHILE_SET_AT_EXECUTE };
        depthParams[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
        depthParams[3].ParameterType = D3D12_ROOT_PARAMETER_TYPE_UAV;
        depthParams[3].Descriptor = { 0, 0, D3D12_ROOT_DESCRIPTOR_FLAG_DATA_VOLATILE };
        depthParams[3].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
        depthParams[4].ParameterType = D3D12_ROOT_PARAMETER_TYPE_UAV;
        depthParams[4].Descriptor = { 1, 0, D3D12_ROOT_DESCRIPTOR_FLAG_DATA_VOLATILE };
        depthParams[4].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
        depthParams[5].ParameterType = D3D12_ROOT_PARAMETER_TYPE_UAV;
        depthParams[5].Descriptor = { 2, 0, D3D12_ROOT_DESCRIPTOR_FLAG_DATA_VOLATILE };
        depthParams[5].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

        D3D12_VERSIONED_ROOT_SIGNATURE_DESC depthRootDesc = {};
        depthRootDesc.Version = D3D_ROOT_SIGNATURE_VERSION_1_1;
        depthRootDesc.Desc_1_1.NumParameters = _countof(depthParams);
        depthRootDesc.Desc_1_1.pParameters = depthParams;
        depthRootDesc.Desc_1_1.Flags = D3D12_ROOT_SIGNATURE_FLAG_NONE;

        ComPtr<ID3DBlob> depthSigBlob, depthErrBlob;
        HRESULT hr = D3D12SerializeVersionedRootSignature(&depthRootDesc, &depthSigBlob, &depthErrBlob);
        if (FAILED(hr)) {
            return false;
        }

        hr = d3dDevice->CreateRootSignature(0, depthSigBlob->GetBufferPointer(), depthSigBlob->GetBufferSize(),
            IID_PPV_ARGS(&resources.coarseDepthRootSignature));
        if (FAILED(hr)) {
            return false;
        }

        ComPtr<ID3DBlob> coarseDepthBlob, depthBinArgsBlob;
        if (!LoadBlob(L"Data/Shader/EffectParticleCoarseDepth_cs.cso", coarseDepthBlob) ||
            !LoadBlob(L"Data/Shader/EffectParticleDepthBinArgs_cs.cso", depthBinArgsBlob)) {
            return false;
        }

        auto createPso = [&](ID3DBlob* blob, ComPtr<ID3D12PipelineState>& outState) -> bool {
            D3D12_COMPUTE_PIPELINE_STATE_DESC psoDesc = {};
            psoDesc.pRootSignature = resources.coarseDepthRootSignature.Get();
            psoDesc.CS = { blob->GetBufferPointer(), blob->GetBufferSize() };
            return SUCCEEDED(d3dDevice->CreateComputePipelineState(&psoDesc, IID_PPV_ARGS(&outState)));
        };

        if (!createPso(coarseDepthBlob.Get(), resources.coarseDepthPipelineState) ||
            !createPso(depthBinArgsBlob.Get(), resources.depthBinArgsPipelineState)) {
            return false;
        }

        D3D12_INDIRECT_ARGUMENT_DESC indirectArg = {};
        indirectArg.Type = D3D12_INDIRECT_ARGUMENT_TYPE_DRAW;
        D3D12_COMMAND_SIGNATURE_DESC cmdSigDesc = {};
        cmdSigDesc.ByteStride = 16u;
        cmdSigDesc.NumArgumentDescs = 1;
        cmdSigDesc.pArgumentDescs = &indirectArg;
        hr = d3dDevice->CreateCommandSignature(&cmdSigDesc, nullptr, IID_PPV_ARGS(&resources.depthBinCommandSignature));
        if (FAILED(hr)) {
            return false;
        }

        return true;
    }

    bool CreateSortPipelines(DX12Device* device, EffectParticleDx12Resources& resources)
    {
        if (!device || (resources.sortB2PipelineState && resources.sortC2PipelineState)) {
            return resources.sortB2PipelineState != nullptr && resources.sortC2PipelineState != nullptr;
        }

        auto* d3dDevice = device->GetDevice();
        if (!d3dDevice) {
            return false;
        }

        D3D12_ROOT_PARAMETER1 params[2] = {};
        params[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
        params[0].Descriptor = { 11, 0, D3D12_ROOT_DESCRIPTOR_FLAG_DATA_STATIC_WHILE_SET_AT_EXECUTE };
        params[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
        params[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_UAV;
        params[1].Descriptor = { 3, 0, D3D12_ROOT_DESCRIPTOR_FLAG_DATA_VOLATILE };
        params[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

        D3D12_VERSIONED_ROOT_SIGNATURE_DESC rootDesc = {};
        rootDesc.Version = D3D_ROOT_SIGNATURE_VERSION_1_1;
        rootDesc.Desc_1_1.NumParameters = _countof(params);
        rootDesc.Desc_1_1.pParameters = params;
        rootDesc.Desc_1_1.Flags = D3D12_ROOT_SIGNATURE_FLAG_NONE;

        ComPtr<ID3DBlob> signatureBlob;
        ComPtr<ID3DBlob> errorBlob;
        HRESULT hr = D3D12SerializeVersionedRootSignature(&rootDesc, &signatureBlob, &errorBlob);
        if (FAILED(hr)) {
            if (errorBlob) {
                LOG_ERROR("[EffectParticlePass] Sort root signature serialize failed: %s",
                    static_cast<const char*>(errorBlob->GetBufferPointer()));
            }
            return false;
        }

        hr = d3dDevice->CreateRootSignature(
            0,
            signatureBlob->GetBufferPointer(),
            signatureBlob->GetBufferSize(),
            IID_PPV_ARGS(&resources.sortRootSignature));
        if (FAILED(hr)) {
            LOG_ERROR("[EffectParticlePass] Failed to create sort root signature");
            return false;
        }

        ComPtr<ID3DBlob> b2Blob;
        ComPtr<ID3DBlob> c2Blob;
        if (!LoadBlob(L"Data/Shader/compute_particle_bitonic_sort_b2_cs.cso", b2Blob) ||
            !LoadBlob(L"Data/Shader/compute_particle_bitonic_sort_c2_cs.cso", c2Blob)) {
            return false;
        }

        D3D12_COMPUTE_PIPELINE_STATE_DESC psoDesc = {};
        psoDesc.pRootSignature = resources.sortRootSignature.Get();
        psoDesc.CS = { b2Blob->GetBufferPointer(), b2Blob->GetBufferSize() };
        hr = d3dDevice->CreateComputePipelineState(&psoDesc, IID_PPV_ARGS(&resources.sortB2PipelineState));
        if (FAILED(hr)) {
            LOG_ERROR("[EffectParticlePass] Failed to create bitonic B2 pipeline state");
            return false;
        }

        psoDesc.CS = { c2Blob->GetBufferPointer(), c2Blob->GetBufferSize() };
        hr = d3dDevice->CreateComputePipelineState(&psoDesc, IID_PPV_ARGS(&resources.sortC2PipelineState));
        if (FAILED(hr)) {
            LOG_ERROR("[EffectParticlePass] Failed to create bitonic C2 pipeline state");
            return false;
        }

        return true;
    }

    bool CreateBillboardPipeline(DX12Device* device, EffectParticleDx12Resources& resources)
    {
        if (!device || (resources.billboardPipelineState && resources.billboardCommandSignature)) {
            return resources.billboardPipelineState != nullptr && resources.billboardCommandSignature != nullptr;
        }

        auto* d3dDevice = device->GetDevice();
        if (!d3dDevice) {
            return false;
        }

        D3D12_DESCRIPTOR_RANGE1 textureRange = {};
        textureRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
        textureRange.NumDescriptors = 2;
        textureRange.BaseShaderRegister = 0;
        textureRange.RegisterSpace = 0;
        textureRange.Flags = D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC_WHILE_SET_AT_EXECUTE;
        textureRange.OffsetInDescriptorsFromTableStart = 0;

        // Billboard root sig: b0(scene), t0(AliveList), t2(BillboardHot), t3(BillboardWarm), b2(render), descriptorTable(textures)
        D3D12_ROOT_PARAMETER1 params[6] = {};
        params[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
        params[0].Descriptor = { 0, 0, D3D12_ROOT_DESCRIPTOR_FLAG_DATA_STATIC_WHILE_SET_AT_EXECUTE };
        params[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
        params[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_SRV;
        params[1].Descriptor = { 0, 0, D3D12_ROOT_DESCRIPTOR_FLAG_DATA_STATIC_WHILE_SET_AT_EXECUTE };
        params[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_GEOMETRY;  // t0 = AliveList
        params[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_SRV;
        params[2].Descriptor = { 2, 0, D3D12_ROOT_DESCRIPTOR_FLAG_DATA_STATIC_WHILE_SET_AT_EXECUTE };
        params[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_GEOMETRY;  // t2 = BillboardHot
        params[3].ParameterType = D3D12_ROOT_PARAMETER_TYPE_SRV;
        params[3].Descriptor = { 3, 0, D3D12_ROOT_DESCRIPTOR_FLAG_DATA_STATIC_WHILE_SET_AT_EXECUTE };
        params[3].ShaderVisibility = D3D12_SHADER_VISIBILITY_GEOMETRY;  // t3 = BillboardWarm
        params[4].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
        params[4].Descriptor = { 2, 0, D3D12_ROOT_DESCRIPTOR_FLAG_DATA_STATIC_WHILE_SET_AT_EXECUTE };
        params[4].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
        params[5].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
        params[5].DescriptorTable.NumDescriptorRanges = 1;
        params[5].DescriptorTable.pDescriptorRanges = &textureRange;
        params[5].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

        D3D12_STATIC_SAMPLER_DESC samplerDesc = {};
        samplerDesc.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
        samplerDesc.AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
        samplerDesc.AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
        samplerDesc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
        samplerDesc.ShaderRegister = 1;
        samplerDesc.RegisterSpace = 0;
        samplerDesc.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
        samplerDesc.ComparisonFunc = D3D12_COMPARISON_FUNC_ALWAYS;
        samplerDesc.MaxLOD = D3D12_FLOAT32_MAX;

        D3D12_VERSIONED_ROOT_SIGNATURE_DESC rootDesc = {};
        rootDesc.Version = D3D_ROOT_SIGNATURE_VERSION_1_1;
        rootDesc.Desc_1_1.NumParameters = _countof(params);
        rootDesc.Desc_1_1.pParameters = params;
        rootDesc.Desc_1_1.NumStaticSamplers = 1;
        rootDesc.Desc_1_1.pStaticSamplers = &samplerDesc;
        rootDesc.Desc_1_1.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

        ComPtr<ID3DBlob> signatureBlob;
        ComPtr<ID3DBlob> errorBlob;
        HRESULT hr = D3D12SerializeVersionedRootSignature(&rootDesc, &signatureBlob, &errorBlob);
        if (FAILED(hr)) {
            if (errorBlob) {
                LOG_ERROR("[EffectParticlePass] Billboard root signature serialize failed: %s",
                    static_cast<const char*>(errorBlob->GetBufferPointer()));
            }
            return false;
        }

        hr = d3dDevice->CreateRootSignature(0, signatureBlob->GetBufferPointer(), signatureBlob->GetBufferSize(), IID_PPV_ARGS(&resources.billboardRootSignature));
        if (FAILED(hr)) {
            LOG_ERROR("[EffectParticlePass] Failed to create billboard root signature");
            return false;
        }

        ComPtr<ID3DBlob> vsBlob;
        ComPtr<ID3DBlob> gsBlob;
        ComPtr<ID3DBlob> psBlob;
        if (!LoadBlob(L"Data/Shader/compute_particle_render_vs.cso", vsBlob) ||
            !LoadBlob(L"Data/Shader/EffectParticleBillboardGS.cso", gsBlob) ||
            !LoadBlob(L"Data/Shader/EffectParticleBillboardPS.cso", psBlob)) {
            return false;
        }

        D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
        psoDesc.pRootSignature = resources.billboardRootSignature.Get();
        psoDesc.VS = { vsBlob->GetBufferPointer(), vsBlob->GetBufferSize() };
        psoDesc.GS = { gsBlob->GetBufferPointer(), gsBlob->GetBufferSize() };
        psoDesc.PS = { psBlob->GetBufferPointer(), psBlob->GetBufferSize() };
        psoDesc.BlendState = CreatePremultipliedAlphaBlendDesc();
        psoDesc.SampleMask = UINT_MAX;
        psoDesc.RasterizerState = CreateRasterDesc(D3D12_CULL_MODE_NONE);
        psoDesc.DepthStencilState = CreateReadOnlyDepthDesc();
        psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_POINT;
        psoDesc.NumRenderTargets = 1;
        psoDesc.RTVFormats[0] = DXGI_FORMAT_R16G16B16A16_FLOAT;
        psoDesc.DSVFormat = DXGI_FORMAT_D32_FLOAT;
        psoDesc.SampleDesc.Count = 1;

        hr = d3dDevice->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&resources.billboardPipelineState));
        if (FAILED(hr)) {
            LOG_ERROR("[EffectParticlePass] Failed to create billboard pipeline state");
            return false;
        }

        // Create blend mode PSO variants
        for (int i = 0; i < static_cast<int>(EffectParticleBlendMode::EnumCount); ++i) {
            psoDesc.BlendState = CreateBlendDesc(static_cast<EffectParticleBlendMode>(i));
            hr = d3dDevice->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&resources.billboardBlendPSOs[i]));
            if (FAILED(hr)) {
                LOG_ERROR("[EffectParticlePass] Failed to create billboard blend PSO %d", i);
                return false;
            }
        }

        D3D12_INDIRECT_ARGUMENT_DESC drawArgumentDesc = {};
        drawArgumentDesc.Type = D3D12_INDIRECT_ARGUMENT_TYPE_DRAW;

        D3D12_COMMAND_SIGNATURE_DESC signatureDesc = {};
        signatureDesc.ByteStride = sizeof(D3D12_DRAW_ARGUMENTS);
        signatureDesc.NumArgumentDescs = 1;
        signatureDesc.pArgumentDescs = &drawArgumentDesc;

        hr = d3dDevice->CreateCommandSignature(&signatureDesc, nullptr, IID_PPV_ARGS(&resources.billboardCommandSignature));
        if (FAILED(hr)) {
            LOG_ERROR("[EffectParticlePass] Failed to create billboard command signature");
            return false;
        }

        return true;
    }

    bool CreateRibbonPipeline(DX12Device* device, EffectParticleDx12Resources& resources)
    {
        if (!device || resources.ribbonPipelineState) {
            return resources.ribbonPipelineState != nullptr;
        }

        auto* d3dDevice = device->GetDevice();
        if (!d3dDevice) {
            return false;
        }

        D3D12_DESCRIPTOR_RANGE1 textureRange = {};
        textureRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
        textureRange.NumDescriptors = 2;
        textureRange.BaseShaderRegister = 0;
        textureRange.RegisterSpace = 0;
        textureRange.Flags = D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC_WHILE_SET_AT_EXECUTE;
        textureRange.OffsetInDescriptorsFromTableStart = 0;

        // Ribbon root sig: [0]=b0 CbScene, [1]=t0 AliveList, [2]=t1 Hot, [3]=t2 Warm,
        //                   [4]=t3 RibbonHistory, [5]=b2 RenderConstants, [6]=descriptor table
        D3D12_ROOT_PARAMETER1 params[7] = {};
        params[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
        params[0].Descriptor = { 0, 0, D3D12_ROOT_DESCRIPTOR_FLAG_DATA_STATIC_WHILE_SET_AT_EXECUTE };
        params[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
        params[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_SRV;
        params[1].Descriptor = { 0, 0, D3D12_ROOT_DESCRIPTOR_FLAG_DATA_STATIC_WHILE_SET_AT_EXECUTE };
        params[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_GEOMETRY;
        params[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_SRV;
        params[2].Descriptor = { 1, 0, D3D12_ROOT_DESCRIPTOR_FLAG_DATA_STATIC_WHILE_SET_AT_EXECUTE };
        params[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_GEOMETRY;
        params[3].ParameterType = D3D12_ROOT_PARAMETER_TYPE_SRV;
        params[3].Descriptor = { 2, 0, D3D12_ROOT_DESCRIPTOR_FLAG_DATA_STATIC_WHILE_SET_AT_EXECUTE };
        params[3].ShaderVisibility = D3D12_SHADER_VISIBILITY_GEOMETRY;
        params[4].ParameterType = D3D12_ROOT_PARAMETER_TYPE_SRV;
        params[4].Descriptor = { 3, 0, D3D12_ROOT_DESCRIPTOR_FLAG_DATA_STATIC_WHILE_SET_AT_EXECUTE };
        params[4].ShaderVisibility = D3D12_SHADER_VISIBILITY_GEOMETRY;
        params[5].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
        params[5].Descriptor = { 2, 0, D3D12_ROOT_DESCRIPTOR_FLAG_DATA_STATIC_WHILE_SET_AT_EXECUTE };
        params[5].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
        params[6].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
        params[6].DescriptorTable.NumDescriptorRanges = 1;
        params[6].DescriptorTable.pDescriptorRanges = &textureRange;
        params[6].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

        D3D12_STATIC_SAMPLER_DESC samplerDesc = {};
        samplerDesc.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
        samplerDesc.AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
        samplerDesc.AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
        samplerDesc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
        samplerDesc.ShaderRegister = 1;
        samplerDesc.RegisterSpace = 0;
        samplerDesc.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
        samplerDesc.ComparisonFunc = D3D12_COMPARISON_FUNC_ALWAYS;
        samplerDesc.MaxLOD = D3D12_FLOAT32_MAX;

        D3D12_VERSIONED_ROOT_SIGNATURE_DESC rootDesc = {};
        rootDesc.Version = D3D_ROOT_SIGNATURE_VERSION_1_1;
        rootDesc.Desc_1_1.NumParameters = _countof(params);
        rootDesc.Desc_1_1.pParameters = params;
        rootDesc.Desc_1_1.NumStaticSamplers = 1;
        rootDesc.Desc_1_1.pStaticSamplers = &samplerDesc;
        rootDesc.Desc_1_1.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

        ComPtr<ID3DBlob> signatureBlob;
        ComPtr<ID3DBlob> errorBlob;
        HRESULT hr = D3D12SerializeVersionedRootSignature(&rootDesc, &signatureBlob, &errorBlob);
        if (FAILED(hr)) {
            if (errorBlob) {
                LOG_ERROR("[EffectParticlePass] Ribbon root signature serialize failed: %s",
                    static_cast<const char*>(errorBlob->GetBufferPointer()));
            }
            return false;
        }

        hr = d3dDevice->CreateRootSignature(0, signatureBlob->GetBufferPointer(), signatureBlob->GetBufferSize(), IID_PPV_ARGS(&resources.ribbonRootSignature));
        if (FAILED(hr)) {
            LOG_ERROR("[EffectParticlePass] Failed to create ribbon root signature");
            return false;
        }

        ComPtr<ID3DBlob> vsBlob;
        ComPtr<ID3DBlob> gsBlob;
        ComPtr<ID3DBlob> psBlob;
        if (!LoadBlob(L"Data/Shader/EffectParticleRibbonVS.cso", vsBlob) ||
            !LoadBlob(L"Data/Shader/EffectParticleRibbonGS.cso", gsBlob) ||
            !LoadBlob(L"Data/Shader/EffectParticleBillboardPS.cso", psBlob)) {
            return false;
        }

        D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
        psoDesc.pRootSignature = resources.ribbonRootSignature.Get();
        psoDesc.VS = { vsBlob->GetBufferPointer(), vsBlob->GetBufferSize() };
        psoDesc.GS = { gsBlob->GetBufferPointer(), gsBlob->GetBufferSize() };
        psoDesc.PS = { psBlob->GetBufferPointer(), psBlob->GetBufferSize() };
        psoDesc.BlendState = CreatePremultipliedAlphaBlendDesc();
        psoDesc.SampleMask = UINT_MAX;
        psoDesc.RasterizerState = CreateRasterDesc(D3D12_CULL_MODE_NONE);
        psoDesc.DepthStencilState = CreateReadOnlyDepthDesc();
        psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_POINT;
        psoDesc.NumRenderTargets = 1;
        psoDesc.RTVFormats[0] = DXGI_FORMAT_R16G16B16A16_FLOAT;
        psoDesc.DSVFormat = DXGI_FORMAT_D32_FLOAT;
        psoDesc.SampleDesc.Count = 1;

        hr = d3dDevice->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&resources.ribbonPipelineState));
        if (FAILED(hr)) {
            LOG_ERROR("[EffectParticlePass] Failed to create ribbon pipeline state");
            return false;
        }

        // Create blend mode PSO variants for ribbon
        for (int i = 0; i < static_cast<int>(EffectParticleBlendMode::EnumCount); ++i) {
            psoDesc.BlendState = CreateBlendDesc(static_cast<EffectParticleBlendMode>(i));
            hr = d3dDevice->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&resources.ribbonBlendPSOs[i]));
            if (FAILED(hr)) {
                LOG_ERROR("[EffectParticlePass] Failed to create ribbon blend PSO %d", i);
                return false;
            }
        }

        return true;
    }

    bool CreateMeshPipeline(DX12Device* device, EffectParticleDx12Resources& resources)
    {
        if (!device || resources.meshPipelineState) {
            return resources.meshPipelineState != nullptr;
        }

        auto* d3dDevice = device->GetDevice();
        if (!d3dDevice) {
            return false;
        }

        // SoA mesh VS consumes: AliveList(t0), BillboardHot(t1), BillboardWarm(t2),
        // BillboardHeader(t3), MeshAttribHot(t4). PS consumes color_map at t5 via table.
        D3D12_DESCRIPTOR_RANGE1 textureRange = {};
        textureRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
        textureRange.NumDescriptors = 1;
        textureRange.BaseShaderRegister = 5;
        textureRange.RegisterSpace = 0;
        textureRange.Flags = D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC_WHILE_SET_AT_EXECUTE;
        textureRange.OffsetInDescriptorsFromTableStart = 0;

        // [0] b0 CBV (CbScene: viewProj/light)
        // [1] b2 CBV (render cbuffer: velocity stretch / global_alpha / curl_noise)
        // [2] t0 SRV (AliveList)            [VS]
        // [3] t1 SRV (BillboardHot)         [VS]
        // [4] t2 SRV (BillboardWarm)        [VS]
        // [5] t3 SRV (BillboardHeader)      [VS]
        // [6] t4 SRV (MeshAttribHot)        [VS]
        // [7] table t5 (color_map)          [PS]
        D3D12_ROOT_PARAMETER1 params[8] = {};
        params[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
        params[0].Descriptor = { 0, 0, D3D12_ROOT_DESCRIPTOR_FLAG_DATA_STATIC_WHILE_SET_AT_EXECUTE };
        params[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
        params[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
        params[1].Descriptor = { 2, 0, D3D12_ROOT_DESCRIPTOR_FLAG_DATA_STATIC_WHILE_SET_AT_EXECUTE };
        params[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
        params[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_SRV;
        params[2].Descriptor = { 0, 0, D3D12_ROOT_DESCRIPTOR_FLAG_DATA_STATIC_WHILE_SET_AT_EXECUTE };
        params[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
        params[3].ParameterType = D3D12_ROOT_PARAMETER_TYPE_SRV;
        params[3].Descriptor = { 1, 0, D3D12_ROOT_DESCRIPTOR_FLAG_DATA_STATIC_WHILE_SET_AT_EXECUTE };
        params[3].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
        params[4].ParameterType = D3D12_ROOT_PARAMETER_TYPE_SRV;
        params[4].Descriptor = { 2, 0, D3D12_ROOT_DESCRIPTOR_FLAG_DATA_STATIC_WHILE_SET_AT_EXECUTE };
        params[4].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
        params[5].ParameterType = D3D12_ROOT_PARAMETER_TYPE_SRV;
        params[5].Descriptor = { 3, 0, D3D12_ROOT_DESCRIPTOR_FLAG_DATA_STATIC_WHILE_SET_AT_EXECUTE };
        params[5].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
        params[6].ParameterType = D3D12_ROOT_PARAMETER_TYPE_SRV;
        params[6].Descriptor = { 4, 0, D3D12_ROOT_DESCRIPTOR_FLAG_DATA_STATIC_WHILE_SET_AT_EXECUTE };
        params[6].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
        params[7].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
        params[7].DescriptorTable.NumDescriptorRanges = 1;
        params[7].DescriptorTable.pDescriptorRanges = &textureRange;
        params[7].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

        D3D12_STATIC_SAMPLER_DESC samplerDesc = {};
        samplerDesc.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
        samplerDesc.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
        samplerDesc.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
        samplerDesc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
        samplerDesc.ShaderRegister = 1;
        samplerDesc.RegisterSpace = 0;
        samplerDesc.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
        samplerDesc.ComparisonFunc = D3D12_COMPARISON_FUNC_ALWAYS;
        samplerDesc.MaxLOD = D3D12_FLOAT32_MAX;

        D3D12_VERSIONED_ROOT_SIGNATURE_DESC rootDesc = {};
        rootDesc.Version = D3D_ROOT_SIGNATURE_VERSION_1_1;
        rootDesc.Desc_1_1.NumParameters = _countof(params);
        rootDesc.Desc_1_1.pParameters = params;
        rootDesc.Desc_1_1.NumStaticSamplers = 1;
        rootDesc.Desc_1_1.pStaticSamplers = &samplerDesc;
        rootDesc.Desc_1_1.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

        ComPtr<ID3DBlob> signatureBlob;
        ComPtr<ID3DBlob> errorBlob;
        HRESULT hr = D3D12SerializeVersionedRootSignature(&rootDesc, &signatureBlob, &errorBlob);
        if (FAILED(hr)) {
            if (errorBlob) {
                LOG_ERROR("[EffectParticlePass] Mesh root signature serialize failed: %s",
                    static_cast<const char*>(errorBlob->GetBufferPointer()));
            }
            return false;
        }

        hr = d3dDevice->CreateRootSignature(0, signatureBlob->GetBufferPointer(), signatureBlob->GetBufferSize(), IID_PPV_ARGS(&resources.meshRootSignature));
        if (FAILED(hr)) {
            LOG_ERROR("[EffectParticlePass] Failed to create mesh particle root signature");
            return false;
        }

        ComPtr<ID3DBlob> vsBlob;
        ComPtr<ID3DBlob> psBlob;
        if (!LoadBlob(L"Data/Shader/EffectParticleMeshVS.cso", vsBlob) ||
            !LoadBlob(L"Data/Shader/EffectParticleMeshPS.cso", psBlob)) {
            return false;
        }

        D3D12_INPUT_ELEMENT_DESC inputLayout[] =
        {
            { "POSITION",     0, DXGI_FORMAT_R32G32B32_FLOAT,    0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
            { "BONE_WEIGHTS", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
            { "BONE_INDICES", 0, DXGI_FORMAT_R32G32B32A32_UINT,  0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
            { "TEXCOORD",     0, DXGI_FORMAT_R32G32_FLOAT,       0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
            { "NORMAL",       0, DXGI_FORMAT_R32G32B32_FLOAT,    0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
            { "TANGENT",      0, DXGI_FORMAT_R32G32B32_FLOAT,    0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
            { "COLOR",        0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        };

        D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
        psoDesc.pRootSignature = resources.meshRootSignature.Get();
        psoDesc.VS = { vsBlob->GetBufferPointer(), vsBlob->GetBufferSize() };
        psoDesc.PS = { psBlob->GetBufferPointer(), psBlob->GetBufferSize() };
        psoDesc.BlendState = CreateAdditiveBlendDesc();
        psoDesc.SampleMask = UINT_MAX;
        psoDesc.RasterizerState = CreateRasterDesc(D3D12_CULL_MODE_BACK);
        psoDesc.DepthStencilState = CreateReadOnlyDepthDesc();
        psoDesc.InputLayout = { inputLayout, _countof(inputLayout) };
        psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
        psoDesc.NumRenderTargets = 1;
        psoDesc.RTVFormats[0] = DXGI_FORMAT_R16G16B16A16_FLOAT;
        psoDesc.DSVFormat = DXGI_FORMAT_D32_FLOAT;
        psoDesc.SampleDesc.Count = 1;

        hr = d3dDevice->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&resources.meshPipelineState));
        if (FAILED(hr)) {
            LOG_ERROR("[EffectParticlePass] Failed to create mesh particle pipeline state");
            return false;
        }

        return true;
    }

    bool CreateTrailPipeline(DX12Device* device, EffectParticleDx12Resources& resources)
    {
        if (!device || resources.trailPipelineState) {
            return resources.trailPipelineState != nullptr;
        }

        auto* d3dDevice = device->GetDevice();
        if (!d3dDevice) {
            return false;
        }

        // Trail root sig: [0]=b0 CbTrail (ViewProjection matrix)
        D3D12_ROOT_PARAMETER1 params[1] = {};
        params[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
        params[0].Descriptor = { 0, 0, D3D12_ROOT_DESCRIPTOR_FLAG_DATA_STATIC_WHILE_SET_AT_EXECUTE };
        params[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

        D3D12_VERSIONED_ROOT_SIGNATURE_DESC rootDesc = {};
        rootDesc.Version = D3D_ROOT_SIGNATURE_VERSION_1_1;
        rootDesc.Desc_1_1.NumParameters = _countof(params);
        rootDesc.Desc_1_1.pParameters = params;
        rootDesc.Desc_1_1.NumStaticSamplers = 0;
        rootDesc.Desc_1_1.pStaticSamplers = nullptr;
        rootDesc.Desc_1_1.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

        ComPtr<ID3DBlob> signatureBlob;
        ComPtr<ID3DBlob> errorBlob;
        HRESULT hr = D3D12SerializeVersionedRootSignature(&rootDesc, &signatureBlob, &errorBlob);
        if (FAILED(hr)) {
            if (errorBlob) {
                LOG_ERROR("[EffectParticlePass] Trail root signature serialize failed: %s",
                    static_cast<const char*>(errorBlob->GetBufferPointer()));
            }
            return false;
        }

        hr = d3dDevice->CreateRootSignature(0, signatureBlob->GetBufferPointer(), signatureBlob->GetBufferSize(), IID_PPV_ARGS(&resources.trailRootSignature));
        if (FAILED(hr)) {
            LOG_ERROR("[EffectParticlePass] Failed to create trail root signature");
            return false;
        }

        ComPtr<ID3DBlob> vsBlob;
        ComPtr<ID3DBlob> psBlob;
        if (!LoadBlob(L"Data/Shader/TrailVS.cso", vsBlob) ||
            !LoadBlob(L"Data/Shader/TrailPS.cso", psBlob)) {
            return false;
        }

        D3D12_INPUT_ELEMENT_DESC inputLayout[] =
        {
            { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT,    0, 0,  D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
            { "COLOR",    0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
            { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,       0, 28, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        };

        D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
        psoDesc.pRootSignature = resources.trailRootSignature.Get();
        psoDesc.VS = { vsBlob->GetBufferPointer(), vsBlob->GetBufferSize() };
        psoDesc.PS = { psBlob->GetBufferPointer(), psBlob->GetBufferSize() };
        psoDesc.BlendState = CreateBlendDesc(EffectParticleBlendMode::PremultipliedAlpha);
        psoDesc.SampleMask = UINT_MAX;
        psoDesc.RasterizerState = CreateRasterDesc(D3D12_CULL_MODE_NONE);
        psoDesc.DepthStencilState = CreateReadOnlyDepthDesc();
        psoDesc.InputLayout = { inputLayout, _countof(inputLayout) };
        psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
        psoDesc.NumRenderTargets = 1;
        psoDesc.RTVFormats[0] = DXGI_FORMAT_R16G16B16A16_FLOAT;
        psoDesc.DSVFormat = DXGI_FORMAT_D32_FLOAT;
        psoDesc.SampleDesc.Count = 1;

        hr = d3dDevice->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&resources.trailPipelineState));
        if (FAILED(hr)) {
            LOG_ERROR("[EffectParticlePass] Failed to create trail pipeline state");
            return false;
        }

        return true;
    }

    bool CreateTextureDescriptorHeap(DX12Device* device, EffectParticleDx12Resources& resources)
    {
        if (!device || resources.textureHeap) {
            return resources.textureHeap != nullptr;
        }

        auto* d3dDevice = device->GetDevice();
        if (!d3dDevice) {
            return false;
        }

        D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
        heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
        heapDesc.NumDescriptors = 3;
        heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
        const HRESULT hr = d3dDevice->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&resources.textureHeap));
        if (FAILED(hr)) {
            LOG_ERROR("[EffectParticlePass] Failed to create particle texture descriptor heap");
            return false;
        }

        resources.textureHeapCpu = resources.textureHeap->GetCPUDescriptorHandleForHeapStart();
        resources.textureHeapGpu = resources.textureHeap->GetGPUDescriptorHandleForHeapStart();
        resources.textureHeapDescriptorSize = d3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
        return true;
    }

    bool EnsureCurlNoiseTexture(DX12Device* device, EffectParticleDx12Resources& resources)
    {
        if (!device || !resources.textureHeap) {
            return false;
        }

        if (!resources.curlNoiseTexture) {
            resources.curlNoiseTexture = CreateCurlNoiseVolumeTexture(device);
            if (!resources.curlNoiseTexture) {
                LOG_ERROR("[EffectParticlePass] Failed to initialize DX12 curl noise volume");
                return false;
            }
        }

        auto* dx12Texture = dynamic_cast<DX12Texture*>(resources.curlNoiseTexture.get());
        if (!dx12Texture || !dx12Texture->HasSRV()) {
            LOG_ERROR("[EffectParticlePass] Curl noise texture is missing an SRV");
            return false;
        }

        auto* d3dDevice = device->GetDevice();
        if (!d3dDevice) {
            return false;
        }

        const D3D12_CPU_DESCRIPTOR_HANDLE curlNoiseHandle = OffsetCpuHandle(resources.textureHeapCpu, resources.textureHeapDescriptorSize, 2u);
        d3dDevice->CopyDescriptorsSimple(1, curlNoiseHandle, dx12Texture->GetSRV(), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
        return true;
    }

    bool EnsurePassResources(DX12Device* device, EffectParticleDx12Resources& resources)
    {
        if (!resources.defaultTexture) {
            resources.defaultTexture = ResourceManager::Instance().GetTexture("Data/Effect/particle/particle.png");
            if (!resources.defaultTexture) {
                resources.defaultTexture = ResourceManager::Instance().GetTexture("Data/Texture/UI/White.png");
            }
        }

        return CreateSimulationPipelines(device, resources) &&
            CreateBinPipelines(device, resources) &&
            CreateCoarseDepthPipelines(device, resources) &&
            CreateSortPipelines(device, resources) &&
            CreateBillboardPipeline(device, resources) &&
            CreateRibbonPipeline(device, resources) &&
            CreateMeshPipeline(device, resources) &&
            CreateTrailPipeline(device, resources) &&
            CreateTextureDescriptorHeap(device, resources) &&
            EnsureCurlNoiseTexture(device, resources);
    }

    uint32_t GetRendererBudget(EffectParticleDrawMode drawMode)
    {
        switch (drawMode) {
        case EffectParticleDrawMode::Mesh:    return kMeshBudgetCapacity;
        case EffectParticleDrawMode::Ribbon:  return kRibbonBudgetCapacity;
        default:                              return kBillboardBudgetCapacity;
        }
    }

    uint32_t& GetRendererAllocatedSlots(ParticleSharedArenaBuffers& arena, EffectParticleDrawMode drawMode)
    {
        switch (drawMode) {
        case EffectParticleDrawMode::Mesh:    return arena.meshAllocatedSlots;
        case EffectParticleDrawMode::Ribbon:  return arena.ribbonAllocatedSlots;
        default:                              return arena.billboardAllocatedSlots;
        }
    }

    ParticleArenaAllocation* EnsureRuntimeAllocation(
        EffectParticleDx12Resources& resources,
        IResourceFactory* factory,
        uint32_t runtimeInstanceId,
        uint32_t maxParticles,
        EffectParticleDrawMode drawMode)
    {
        if (!factory || runtimeInstanceId == 0 || maxParticles == 0) {
            return nullptr;
        }

        // Per-renderer budget enforcement
        const uint32_t budget = GetRendererBudget(drawMode);
        uint32_t& allocatedSlots = GetRendererAllocatedSlots(resources.sharedArena, drawMode);
        const uint32_t clampedMax = (std::min)(maxParticles, budget);

        auto& allocation = resources.runtimeAllocations[runtimeInstanceId];
        allocation.rendererType = drawMode;

        const uint32_t requiredCapacity = AlignParticleCapacity(clampedMax);
        const uint32_t requiredPages = ComputeParticlePageCount(requiredCapacity);

        // Check if this allocation would exceed renderer budget
        const uint32_t otherAllocated = (allocatedSlots >= allocation.capacity) ? (allocatedSlots - allocation.capacity) : 0u;
        if (otherAllocated + requiredCapacity > budget) {
            const uint32_t remaining = (budget > otherAllocated) ? (budget - otherAllocated) : 0u;
            if (remaining == 0u) {
                return nullptr;  // budget exhausted
            }
            // fall through with reduced capacity handled by AlignParticleCapacity
        }

        if (!allocation.counterBuffer) {
            allocation.counterBuffer = factory->CreateBuffer(256u, BufferType::UAVStorage, nullptr);
            allocation.counterState = D3D12_RESOURCE_STATE_COMMON;
        }
        if (!allocation.indirectArgsBuffer) {
            allocation.indirectArgsBuffer = factory->CreateBuffer(static_cast<uint32_t>(sizeof(D3D12_DRAW_ARGUMENTS)), BufferType::UAVStorage, nullptr);
            allocation.indirectArgsState = D3D12_RESOURCE_STATE_COMMON;
        }
        if (!allocation.counterBuffer || !allocation.indirectArgsBuffer) {
            return nullptr;
        }

        const bool needsResize = allocation.pageCount < requiredPages;
        if (needsResize && allocation.pageCount > 0u) {
            allocatedSlots -= (std::min)(allocatedSlots, allocation.capacity);
            ReleaseArenaPages(resources.sharedArena, allocation);
            allocation.basePage = 0u;
            allocation.pageCount = 0u;
            allocation.baseSlot = 0u;
            allocation.capacity = 0u;
            allocation.initialized = false;
            ResetEmitterSimulationState(allocation);
        }

        if (allocation.pageCount == 0u) {
            uint32_t totalReservedPages = requiredPages;
            for (const auto& [otherRuntimeId, otherAllocation] : resources.runtimeAllocations) {
                if (otherRuntimeId == runtimeInstanceId) {
                    continue;
                }
                totalReservedPages += otherAllocation.pageCount;
            }

            if (!EnsureSharedArenaCapacity(resources, factory, totalReservedPages)) {
                return nullptr;
            }

            uint32_t basePage = 0u;
            if (!ReserveArenaPages(resources.sharedArena, runtimeInstanceId, requiredPages, basePage)) {
                if (!EnsureSharedArenaCapacity(resources, factory, resources.sharedArena.totalPages + requiredPages)) {
                    return nullptr;
                }
                if (!ReserveArenaPages(resources.sharedArena, runtimeInstanceId, requiredPages, basePage)) {
                    return nullptr;
                }
            }

            allocation.basePage = basePage;
            allocation.pageCount = requiredPages;
            allocation.baseSlot = basePage * kEffectParticlePageSize;
            allocation.capacity = requiredPages * kEffectParticlePageSize;
            allocation.initialized = false;
            ResetEmitterSimulationState(allocation);
            allocatedSlots += allocation.capacity;
        }

        return &allocation;
    }

    DX12Texture* ResolveParticleTexture(EffectParticleDx12Resources& resources, const EffectParticlePacket& packet)
    {
        auto* dx12Texture = dynamic_cast<DX12Texture*>(packet.texture.get());
        if (dx12Texture && dx12Texture->HasSRV()) {
            return dx12Texture;
        }

        dx12Texture = dynamic_cast<DX12Texture*>(resources.defaultTexture.get());
        if (dx12Texture && dx12Texture->HasSRV()) {
            return dx12Texture;
        }

        return nullptr;
    }

    bool DispatchBitonicSort(
        DX12CommandList* dx12CommandList,
        ID3D12GraphicsCommandList* nativeCommandList,
        EffectParticleDx12Resources& resources,
        DX12Buffer* headerBuffer,
        uint32_t particleCapacity)
    {
        if (!dx12CommandList || !nativeCommandList || !headerBuffer || particleCapacity < 2u) {
            return false;
        }

        const uint32_t exponent = static_cast<uint32_t>(std::ceil(std::log2(static_cast<float>(particleCapacity))));
        if (exponent == 0u) {
            return true;
        }

        nativeCommandList->SetComputeRootSignature(resources.sortRootSignature.Get());

        for (uint32_t i = 0; i < exponent; ++i) {
            uint32_t increment = 1u << i;
            for (uint32_t j = 0; j < i + 1; ++j) {
                EffectParticleSortConstants constants;
                constants.increment = increment;
                constants.direction = 2u << i;
                const auto constantsAllocation = dx12CommandList->AllocateDynamicConstantBuffer(
                    &constants,
                    static_cast<uint32_t>(sizeof(constants)));

                const bool useC2 = increment <= 512u;
                nativeCommandList->SetPipelineState(useC2 ? resources.sortC2PipelineState.Get() : resources.sortB2PipelineState.Get());
                nativeCommandList->SetComputeRootConstantBufferView(0, constantsAllocation.gpuVA);
                nativeCommandList->SetComputeRootUnorderedAccessView(1, headerBuffer->GetGPUVirtualAddress());

                if (useC2) {
                    nativeCommandList->Dispatch((std::max)(1u, particleCapacity / 2u / 512u), 1u, 1u);
                    AddUavBarrier(nativeCommandList, headerBuffer->GetNativeResource());
                    break;
                }

                nativeCommandList->Dispatch((std::max)(1u, particleCapacity / 2u / 256u), 1u, 1u);
                AddUavBarrier(nativeCommandList, headerBuffer->GetNativeResource());
                increment >>= 1u;
            }
        }

        return true;
    }

    void FillSceneConstants(EffectParticleSceneConstants& sceneConstants, const RenderContext& rc)
    {
        using namespace DirectX;

        const XMMATRIX view = XMLoadFloat4x4(&rc.viewMatrix);
        const XMMATRIX projection = XMLoadFloat4x4(&rc.projectionMatrix);
        const XMMATRIX inverseView = XMMatrixInverse(nullptr, view);
        XMStoreFloat4x4(&sceneConstants.viewProjection, view * projection);
        XMStoreFloat4x4(&sceneConstants.inverseView, inverseView);

        sceneConstants.lightDirection = {
            rc.directionalLight.direction.x,
            rc.directionalLight.direction.y,
            rc.directionalLight.direction.z,
            0.0f
        };
        sceneConstants.lightColor = {
            rc.directionalLight.color.x,
            rc.directionalLight.color.y,
            rc.directionalLight.color.z,
            1.0f
        };
        sceneConstants.cameraPosition = {
            rc.cameraPosition.x,
            rc.cameraPosition.y,
            rc.cameraPosition.z,
            1.0f
        };

        if (rc.shadowMap) {
            sceneConstants.lightViewProjection = rc.shadowMap->GetLightViewProjection(0);
        } else {
            XMStoreFloat4x4(&sceneConstants.lightViewProjection, XMMatrixIdentity());
        }
    }
}

void EffectParticlePass::Setup(FrameGraphBuilder& builder, const RenderContext&)
{
    m_hSceneColor = builder.GetHandle("SceneColor");
    m_hDepth = builder.GetHandle("GBufferDepth");

    if (m_hSceneColor.IsValid()) {
        m_hSceneColor = builder.Write(m_hSceneColor);
        builder.RegisterHandle("SceneColor", m_hSceneColor);
    }
    if (m_hDepth.IsValid()) {
        builder.Read(m_hDepth);
    }
}

void EffectParticlePass::Execute(FrameGraphResources& resources, const RenderQueue& queue, RenderContext& rc)
{
    if (queue.effectParticlePackets.empty()) {
        return;
    }

    auto& particleResources = GetEffectParticleDx12Resources();
    if (Graphics::Instance().GetAPI() != GraphicsAPI::DX12) {
        if (!particleResources.warnedNonDx12) {
            LOG_ERROR("[EffectParticlePass] DX12 compute particles are only available on the DX12 backend");
            particleResources.warnedNonDx12 = true;
        }
        return;
    }

    auto* dx12Device = Graphics::Instance().GetDX12Device();
    auto* dx12CommandList = dynamic_cast<DX12CommandList*>(rc.commandList);
    auto* factory = Graphics::Instance().GetResourceFactory();
    ITexture* rtScene = resources.GetTexture(m_hSceneColor);
    ITexture* dsReal = resources.GetTexture(m_hDepth);
    if (!dx12Device || !dx12CommandList || !factory || !rtScene || !dsReal) {
        return;
    }

    if (!EnsurePassResources(dx12Device, particleResources)) {
        return;
    }

    auto* nativeCommandList = dx12CommandList->GetNativeCommandList();
    if (!nativeCommandList) {
        return;
    }

    dx12CommandList->FlushResourceBarriers();
    particleResources.frameCounter++;

    ID3D12DescriptorHeap* descriptorHeaps[] = { particleResources.textureHeap.Get() };
    nativeCommandList->SetDescriptorHeaps(1, descriptorHeaps);
    const D3D12_GPU_DESCRIPTOR_HANDLE curlNoiseGpuHandle =
        OffsetGpuHandle(particleResources.textureHeapGpu, particleResources.textureHeapDescriptorSize, 2u);

    std::vector<const EffectParticlePacket*> sortedPackets;
    sortedPackets.reserve(queue.effectParticlePackets.size());
    for (const auto& packet : queue.effectParticlePackets) {
        if (packet.runtimeInstanceId == 0 || packet.maxParticles == 0) {
            continue;
        }
        if (packet.drawMode == EffectParticleDrawMode::Mesh && !packet.modelResource) {
            if (!particleResources.warnedMissingMesh) {
                LOG_ERROR("[EffectParticlePass] Mesh particle mode requires a valid model resource");
                particleResources.warnedMissingMesh = true;
            }
            continue;
        }
        sortedPackets.push_back(&packet);
    }

    PARTICLE_LOG("[EffectParticle] packets=%zu sorted=%zu", queue.effectParticlePackets.size(), sortedPackets.size());

    if (sortedPackets.empty()) {
        return;
    }

    std::unordered_map<uint32_t, uint32_t> requiredPagesPerRuntime;
    requiredPagesPerRuntime.reserve(particleResources.runtimeAllocations.size() + sortedPackets.size());
    for (const auto& [runtimeInstanceId, allocation] : particleResources.runtimeAllocations) {
        if (allocation.pageCount > 0u) {
            requiredPagesPerRuntime[runtimeInstanceId] = allocation.pageCount;
        }
    }

    uint32_t requiredArenaPages = 0u;
    for (const auto* packet : sortedPackets) {
        const uint32_t requiredPages = ComputeParticlePageCount(packet->maxParticles);
        auto& pageCount = requiredPagesPerRuntime[packet->runtimeInstanceId];
        pageCount = (std::max)(pageCount, requiredPages);
    }
    for (const auto& [runtimeInstanceId, pageCount] : requiredPagesPerRuntime) {
        requiredArenaPages += pageCount;
    }
    if (!EnsureSharedArenaCapacity(particleResources, factory, (std::max)(requiredArenaPages, 1u))) {
        return;
    }

    auto* sharedHotBuffer = static_cast<DX12Buffer*>(particleResources.sharedArena.billboardHotBuffer.get());
    auto* sharedWarmBuffer = static_cast<DX12Buffer*>(particleResources.sharedArena.billboardWarmBuffer.get());
    auto* sharedColdBuffer = static_cast<DX12Buffer*>(particleResources.sharedArena.billboardColdBuffer.get());
    auto* sharedHeaderBuffer = static_cast<DX12Buffer*>(particleResources.sharedArena.billboardHeaderBuffer.get());
    auto* sharedMeshAttribHotBuffer = static_cast<DX12Buffer*>(particleResources.sharedArena.meshAttribHotBuffer.get());
    auto* sharedRibbonHistoryBuffer = static_cast<DX12Buffer*>(particleResources.sharedArena.ribbonHistoryBuffer.get());
    auto* sharedDeadListBuffer = static_cast<DX12Buffer*>(particleResources.sharedArena.deadListBuffer.get());
    auto* sharedAliveListBuffer = static_cast<DX12Buffer*>(particleResources.sharedArena.aliveListBuffer.get());
    auto* sharedPageAliveCountBuffer = static_cast<DX12Buffer*>(particleResources.sharedArena.pageAliveCountBuffer.get());
    auto* sharedPageAliveOffsetBuffer = static_cast<DX12Buffer*>(particleResources.sharedArena.pageAliveOffsetBuffer.get());
    if (!sharedHotBuffer || !sharedWarmBuffer || !sharedColdBuffer || !sharedHeaderBuffer ||
        !sharedMeshAttribHotBuffer ||
        !sharedRibbonHistoryBuffer || !sharedDeadListBuffer ||
        !sharedAliveListBuffer || !sharedPageAliveCountBuffer || !sharedPageAliveOffsetBuffer ||
        !sharedHotBuffer->IsValid() || !sharedWarmBuffer->IsValid() ||
        !sharedColdBuffer->IsValid() || !sharedHeaderBuffer->IsValid() ||
        !sharedMeshAttribHotBuffer->IsValid() ||
        !sharedRibbonHistoryBuffer->IsValid() || !sharedDeadListBuffer->IsValid() ||
        !sharedAliveListBuffer->IsValid()) {
        return;
    }

    std::stable_sort(sortedPackets.begin(), sortedPackets.end(),
        [&rc](const EffectParticlePacket* a, const EffectParticlePacket* b) {
            const uint32_t priorityA = GetParticleSortPriority(a->sortMode);
            const uint32_t priorityB = GetParticleSortPriority(b->sortMode);
            if (priorityA != priorityB) {
                return priorityA < priorityB;
            }

            const float distA = ComputeDistanceSqToCamera(*a, rc);
            const float distB = ComputeDistanceSqToCamera(*b, rc);
            if (a->sortMode == EffectParticleSortMode::BackToFront) {
                return distA > distB;
            }
            if (a->sortMode == EffectParticleSortMode::FrontToBack) {
                return distA < distB;
            }
            return false;
        });

    std::vector<BillboardDrawEntry> billboardDrawEntries;
    std::vector<RibbonDrawEntry> ribbonDrawEntries;
    std::vector<MeshDrawEntry> meshDrawEntries;
    billboardDrawEntries.reserve(sortedPackets.size());
    ribbonDrawEntries.reserve(sortedPackets.size());
    meshDrawEntries.reserve(sortedPackets.size());

    auto makeSimulationConstants = [&](const EffectParticlePacket& packet, const ParticleArenaAllocation& runtimeBuffers, float deltaTime, float particleLifetime, uint32_t dispatchCount)
    {
        EffectParticleSimulationConstants constants{};
        constants.originCurrentTime = { packet.origin.x, packet.origin.y, packet.origin.z, packet.currentTime };
        constants.tint = packet.tint;
        constants.tintEnd = packet.tintEnd;
        constants.cameraPositionSortSign = {
            rc.cameraPosition.x,
            rc.cameraPosition.y,
            rc.cameraPosition.z,
            packet.sortMode == EffectParticleSortMode::FrontToBack ? -1.0f : 1.0f
        };
        constants.cameraDirectionCapacity = {
            rc.cameraDirection.x,
            rc.cameraDirection.y,
            rc.cameraDirection.z,
            static_cast<float>(runtimeBuffers.capacity)
        };
        constants.accelerationDrag = {
            packet.acceleration.x,
            packet.acceleration.y,
            packet.acceleration.z,
            packet.drag
        };
        constants.shapeParametersSizeBias = {
            packet.shapeParameters.x,
            packet.shapeParameters.y,
            packet.shapeParameters.z,
            packet.sizeCurveBias
        };
        constants.shapeTypeSpinAlphaBias = {
            static_cast<float>(packet.shapeType),
            packet.spinRate,
            packet.alphaCurveBias,
            0.0f
        };
        constants.timing = {
            deltaTime,
            particleLifetime,
            packet.speed,
            static_cast<float>(dispatchCount)
        };

        const float startSize = (packet.drawMode == EffectParticleDrawMode::Ribbon) ? (std::max)(packet.ribbonWidth, 0.02f) : packet.startSize;
        const float endSize = (packet.drawMode == EffectParticleDrawMode::Ribbon) ? (std::max)(packet.ribbonWidth * 0.35f, 0.01f) : packet.endSize;
        constants.sizeSeed = {
            startSize,
            endSize,
            static_cast<float>(packet.seed != 0 ? packet.seed : 1u),
            0.0f
        };
        constants.subUvParams = {
            static_cast<float>((std::max)(packet.subUvColumns, 1u)),
            static_cast<float>((std::max)(packet.subUvRows, 1u)),
            (std::max)(packet.subUvFrameRate, 0.0f),
            0.0f
        };
        constants.motionParams = {
            (std::max)(packet.curlNoiseStrength, 0.0f),
            (std::max)(packet.curlNoiseScale, 0.01f),
            (std::max)(packet.curlNoiseScrollSpeed, 0.0f),
            packet.vortexStrength
        };
        constants.randomParams = {
            packet.randomSpeedRange,
            packet.randomSizeRange,
            packet.randomLifeRange,
            packet.windStrength
        };
        constants.windDirection = {
            packet.windDirection.x,
            packet.windDirection.y,
            packet.windDirection.z,
            packet.windTurbulence
        };
        constants.sizeCurveValues = packet.sizeCurveValues;
        constants.sizeCurveTimes = packet.sizeCurveTimes;
        constants.gradientColor0 = packet.gradientColor0;
        constants.gradientColor1 = packet.gradientColor1;
        constants.gradientColor2 = packet.gradientColor2;
        constants.gradientColor3 = packet.gradientColor3;
        constants.gradientTimes = {
            0.0f,
            packet.gradientMidTimes.x,
            packet.gradientMidTimes.y,
            1.0f
        };
        // Phase 2: Attractors
        for (int ai = 0; ai < 4; ++ai) {
            (&constants.attractor0)[ai] = packet.attractors[ai];
        }
        constants.attractorRadii = packet.attractorRadii;
        constants.attractorFalloff = packet.attractorFalloff;
        // Phase 2: Collision
        constants.collisionPlane = packet.collisionPlane;
        for (int ci = 0; ci < 4; ++ci) {
            (&constants.collisionSphere0)[ci] = packet.collisionSpheres[ci];
        }
        constants.collisionParams = {
            packet.collisionRestitution,
            packet.collisionFriction,
            static_cast<float>(packet.collisionSphereCount),
            static_cast<float>(packet.attractorCount)
        };
        // MeshParticle Phase 2: copy packet mesh fields into the compute cbuffer.
        // meshFlags.x == 1.0 turns on the mesh-mode branch in Emit/Update CS;
        // zero for Billboard/Ribbon keeps the existing fast path unchanged.
        constants.meshInitialScale = {
            packet.meshInitialScale.x,
            packet.meshInitialScale.y,
            packet.meshInitialScale.z,
            (std::max)(0.0f, packet.meshScaleRandom)
        };
        constants.meshAngularAxisSpeed = {
            packet.meshAngularAxis.x,
            packet.meshAngularAxis.y,
            packet.meshAngularAxis.z,
            packet.meshAngularSpeed
        };
        constants.meshAngularRandomOrient = {
            packet.meshAngularOrientRandom.x,
            packet.meshAngularOrientRandom.y,
            packet.meshAngularOrientRandom.z,
            (std::max)(0.0f, packet.meshAngularSpeedRandom)
        };
        constants.meshFlags = {
            packet.drawMode == EffectParticleDrawMode::Mesh ? 1.0f : 0.0f,
            0.0f, 0.0f, 0.0f
        };
        return constants;
    };

    // SoA binding: [0]b0, [1]t0, [2]u0=Hot, [3]u1=Warm, [4]u2=Cold,
    // [5]u3=Header, [6]u4=Dead, [7]u5=Counter, [8]u6=Ribbon, [9]u7=PageAlive, [10]CurlNoise
    // meshAttribHotGpuVa is always valid (allocated by EnsureSharedArenaCapacity alongside billboards)
    // so we can use it as a hard fallback when no billboard buffer is bound.
    const D3D12_GPU_VIRTUAL_ADDRESS meshAttribHotFallbackGpuVa = sharedMeshAttribHotBuffer->GetGPUVirtualAddress();
    auto bindSimulationResources = [&](const EffectParticleSimulationConstants& constants,
        D3D12_GPU_VIRTUAL_ADDRESS srvGpuVa,
        D3D12_GPU_VIRTUAL_ADDRESS hotGpuVa,
        D3D12_GPU_VIRTUAL_ADDRESS warmGpuVa,
        D3D12_GPU_VIRTUAL_ADDRESS coldGpuVa,
        D3D12_GPU_VIRTUAL_ADDRESS headerGpuVaLocal,
        D3D12_GPU_VIRTUAL_ADDRESS deadGpuVa,
        D3D12_GPU_VIRTUAL_ADDRESS counterGpuVaLocal,
        D3D12_GPU_VIRTUAL_ADDRESS ribbonHistoryGpuVaLocal,
        D3D12_GPU_VIRTUAL_ADDRESS pageAliveCountGpuVaLocal,
        D3D12_GPU_VIRTUAL_ADDRESS meshAttribHotGpuVaLocal)
    {
        const auto allocation = dx12CommandList->AllocateDynamicConstantBuffer(&constants, static_cast<uint32_t>(sizeof(constants)));
        // DX12 requires non-null root descriptors - find any valid address as fallback
        D3D12_GPU_VIRTUAL_ADDRESS fb = hotGpuVa;
        if (fb == 0ull) fb = warmGpuVa;
        if (fb == 0ull) fb = coldGpuVa;
        if (fb == 0ull) fb = headerGpuVaLocal;
        if (fb == 0ull) fb = counterGpuVaLocal;
        if (fb == 0ull) fb = meshAttribHotFallbackGpuVa; // always valid
        nativeCommandList->SetComputeRootSignature(particleResources.simulationRootSignature.Get());
        nativeCommandList->SetComputeRootConstantBufferView(0, allocation.gpuVA);
        nativeCommandList->SetComputeRootShaderResourceView(1, srvGpuVa != 0ull ? srvGpuVa : fb);
        nativeCommandList->SetComputeRootUnorderedAccessView(2, hotGpuVa != 0ull ? hotGpuVa : fb);
        nativeCommandList->SetComputeRootUnorderedAccessView(3, warmGpuVa != 0ull ? warmGpuVa : fb);
        nativeCommandList->SetComputeRootUnorderedAccessView(4, coldGpuVa != 0ull ? coldGpuVa : fb);
        nativeCommandList->SetComputeRootUnorderedAccessView(5, headerGpuVaLocal != 0ull ? headerGpuVaLocal : fb);
        nativeCommandList->SetComputeRootUnorderedAccessView(6, deadGpuVa != 0ull ? deadGpuVa : fb);
        nativeCommandList->SetComputeRootUnorderedAccessView(7, counterGpuVaLocal != 0ull ? counterGpuVaLocal : fb);
        nativeCommandList->SetComputeRootUnorderedAccessView(8, ribbonHistoryGpuVaLocal != 0ull ? ribbonHistoryGpuVaLocal : fb);
        nativeCommandList->SetComputeRootUnorderedAccessView(9, pageAliveCountGpuVaLocal != 0ull ? pageAliveCountGpuVaLocal : fb);
        nativeCommandList->SetComputeRootUnorderedAccessView(10, meshAttribHotGpuVaLocal != 0ull ? meshAttribHotGpuVaLocal : meshAttribHotFallbackGpuVa);
        nativeCommandList->SetComputeRootDescriptorTable(11, curlNoiseGpuHandle);
    };

    for (const EffectParticlePacket* packet : sortedPackets) {
        ParticleArenaAllocation* runtimeBuffers = EnsureRuntimeAllocation(particleResources, factory, packet->runtimeInstanceId, packet->maxParticles, packet->drawMode);
        if (!runtimeBuffers) {
            continue;
        }
        if (!EnsureCounterReadbackBuffers(dx12Device, *runtimeBuffers)) {
            continue;
        }
        ConsumeCounterReadback(*runtimeBuffers, particleResources.frameCounter);

        auto* counterBuffer = static_cast<DX12Buffer*>(runtimeBuffers->counterBuffer.get());
        auto* indirectArgsBuffer = static_cast<DX12Buffer*>(runtimeBuffers->indirectArgsBuffer.get());
        if (!counterBuffer || !indirectArgsBuffer || !counterBuffer->IsValid() || !indirectArgsBuffer->IsValid()) {
            continue;
        }

        const uint64_t baseSlot64 = static_cast<uint64_t>(runtimeBuffers->baseSlot);
        const D3D12_GPU_VIRTUAL_ADDRESS hotGpuVa = OffsetGpuVirtualAddress(sharedHotBuffer, baseSlot64 * kBillboardHotStride);
        const D3D12_GPU_VIRTUAL_ADDRESS warmGpuVa = OffsetGpuVirtualAddress(sharedWarmBuffer, baseSlot64 * kBillboardWarmStride);
        const D3D12_GPU_VIRTUAL_ADDRESS coldGpuVa = OffsetGpuVirtualAddress(sharedColdBuffer, baseSlot64 * kBillboardColdStride);
        const D3D12_GPU_VIRTUAL_ADDRESS headerGpuVa = OffsetGpuVirtualAddress(sharedHeaderBuffer, baseSlot64 * kBillboardHeaderStride);
        const D3D12_GPU_VIRTUAL_ADDRESS meshAttribHotGpuVa = OffsetGpuVirtualAddress(sharedMeshAttribHotBuffer, baseSlot64 * kMeshAttribHotStride);
        const D3D12_GPU_VIRTUAL_ADDRESS ribbonHistoryGpuVa = OffsetGpuVirtualAddress(sharedRibbonHistoryBuffer, baseSlot64 * kEffectParticleRibbonHistoryLength * sizeof(DirectX::XMFLOAT4));
        const D3D12_GPU_VIRTUAL_ADDRESS deadListGpuVa = OffsetGpuVirtualAddress(sharedDeadListBuffer, baseSlot64 * sizeof(uint32_t));
        const D3D12_GPU_VIRTUAL_ADDRESS counterGpuVa = counterBuffer->GetGPUVirtualAddress();
        const D3D12_GPU_VIRTUAL_ADDRESS aliveListGpuVa = sharedAliveListBuffer->GetGPUVirtualAddress();
        const D3D12_GPU_VIRTUAL_ADDRESS pageAliveCountGpuVa = sharedPageAliveCountBuffer->GetGPUVirtualAddress();
        const D3D12_GPU_VIRTUAL_ADDRESS pageAliveOffsetGpuVa = sharedPageAliveOffsetBuffer->GetGPUVirtualAddress();
        auto* sharedBinCounterBuffer = dynamic_cast<DX12Buffer*>(particleResources.sharedArena.binCounterBuffer.get());
        auto* sharedBinIndexBuffer = dynamic_cast<DX12Buffer*>(particleResources.sharedArena.binIndexBuffer.get());
        auto* sharedBinOffsetBuffer = dynamic_cast<DX12Buffer*>(particleResources.sharedArena.binOffsetBuffer.get());
        auto* sharedBinIndirectArgsBuffer = dynamic_cast<DX12Buffer*>(particleResources.sharedArena.binIndirectArgsBuffer.get());
        const D3D12_GPU_VIRTUAL_ADDRESS binCounterGpuVa = sharedBinCounterBuffer ? sharedBinCounterBuffer->GetGPUVirtualAddress() : 0ull;
        const D3D12_GPU_VIRTUAL_ADDRESS binIndexGpuVa = sharedBinIndexBuffer ? sharedBinIndexBuffer->GetGPUVirtualAddress() : 0ull;
        const D3D12_GPU_VIRTUAL_ADDRESS binOffsetGpuVa = sharedBinOffsetBuffer ? sharedBinOffsetBuffer->GetGPUVirtualAddress() : 0ull;
        const D3D12_GPU_VIRTUAL_ADDRESS binIndirectArgsGpuVa = sharedBinIndirectArgsBuffer ? sharedBinIndirectArgsBuffer->GetGPUVirtualAddress() : 0ull;
        auto* sharedDepthBinCounterBuffer = dynamic_cast<DX12Buffer*>(particleResources.sharedArena.depthBinCounterBuffer.get());
        auto* sharedDepthBinIndexBuffer = dynamic_cast<DX12Buffer*>(particleResources.sharedArena.depthBinIndexBuffer.get());
        auto* sharedDepthBinIndirectArgsBuffer = dynamic_cast<DX12Buffer*>(particleResources.sharedArena.depthBinIndirectArgsBuffer.get());
        const D3D12_GPU_VIRTUAL_ADDRESS depthBinCounterGpuVa = sharedDepthBinCounterBuffer ? sharedDepthBinCounterBuffer->GetGPUVirtualAddress() : 0ull;
        const D3D12_GPU_VIRTUAL_ADDRESS depthBinIndexGpuVa = sharedDepthBinIndexBuffer ? sharedDepthBinIndexBuffer->GetGPUVirtualAddress() : 0ull;
        const D3D12_GPU_VIRTUAL_ADDRESS depthBinIndirectArgsGpuVa = sharedDepthBinIndirectArgsBuffer ? sharedDepthBinIndirectArgsBuffer->GetGPUVirtualAddress() : 0ull;

        runtimeBuffers->lastSeenFrame = particleResources.frameCounter;
        const uint32_t seed = packet->seed != 0 ? packet->seed : 1u;
        const float particleLifetime = (std::max)(packet->particleLifetime, 0.001f);
        const float currentTime = (std::max)(packet->currentTime, 0.0f);
        const bool timeWrapped = currentTime + 1.0e-4f < runtimeBuffers->lastSimTime;
        const bool needsReset = !runtimeBuffers->initialized || timeWrapped || runtimeBuffers->lastSeed != seed;

        if (needsReset) {
            ResetEmitterSimulationState(*runtimeBuffers);
            runtimeBuffers->initialized = false;
        }

        const uint32_t activeCountBeforeEmit = runtimeBuffers->lastCompletedCounters.aliveBillboard;
        const float simulationDeltaTime = (std::max)(0.0f, currentTime - runtimeBuffers->lastSimTime);

        uint32_t emitCount = 0;
        if (!runtimeBuffers->burstConsumed && packet->burstCount > 0u) {
            emitCount += packet->burstCount;
            runtimeBuffers->burstConsumed = true;
        }

        if (packet->spawnRate > 0.0f && simulationDeltaTime > 0.0f) {
            const float emitAccumulator = runtimeBuffers->spawnAccumulator + packet->spawnRate * simulationDeltaTime;
            const float wholeParticles = std::floor(emitAccumulator);
            emitCount += static_cast<uint32_t>(wholeParticles);
            runtimeBuffers->spawnAccumulator = emitAccumulator - wholeParticles;
        }

        if (emitCount > 0u) {
            const uint32_t availableDeadCount = runtimeBuffers->initialized
                ? runtimeBuffers->lastCompletedCounters.deadStackTop
                : runtimeBuffers->capacity;
            emitCount = (std::min)(emitCount, availableDeadCount);
        }

        // Use GPU readback when available; fall back to CPU estimate for first frames
        const uint32_t estimatedAliveBase = (activeCountBeforeEmit > 0u)
            ? activeCountBeforeEmit
            : runtimeBuffers->lastEstimatedAlive;
        const uint32_t estimatedPostEmitCount = (std::min)(runtimeBuffers->capacity, estimatedAliveBase + emitCount);
        uint32_t drawCount = runtimeBuffers->lastCompletedCounters.aliveBillboard;
        if (drawCount == 0u && estimatedPostEmitCount > 0u) {
            drawCount = estimatedPostEmitCount;
        }
        const bool needsSimulation = !runtimeBuffers->initialized || estimatedAliveBase > 0u || emitCount > 0u;

        PARTICLE_LOG("[EffectParticle] id=%u cap=%u pages=%u emit=%u active=%u estAlive=%u draw=%u needsSim=%d simDt=%.4f time=%.3f init=%d spawnRate=%.1f burst=%u",
            packet->runtimeInstanceId, runtimeBuffers->capacity, runtimeBuffers->pageCount,
            emitCount, activeCountBeforeEmit, runtimeBuffers->lastEstimatedAlive, drawCount,
            needsSimulation ? 1 : 0, simulationDeltaTime, currentTime,
            runtimeBuffers->initialized ? 1 : 0, packet->spawnRate, packet->burstCount);

        if (runtimeBuffers->pageCount > 0u) {
            const uint32_t livePerPage = estimatedPostEmitCount / runtimeBuffers->pageCount;
            uint32_t liveRemainder = estimatedPostEmitCount % runtimeBuffers->pageCount;
            for (uint32_t pageOffset = 0u; pageOffset < runtimeBuffers->pageCount; ++pageOffset) {
                auto& page = particleResources.sharedArena.pageTable[runtimeBuffers->basePage + pageOffset];
                const uint32_t pageLiveCount = livePerPage + (liveRemainder > 0u ? 1u : 0u);
                if (liveRemainder > 0u) {
                    liveRemainder--;
                }
                page.lastTouchedFrame = static_cast<uint32_t>(particleResources.frameCounter);
                page.liveCount = pageLiveCount;
                page.deadCount = (page.capacity > pageLiveCount) ? (page.capacity - pageLiveCount) : 0u;
                page.occupancyQ16 = static_cast<uint32_t>((static_cast<uint64_t>(page.liveCount) << 16) / (std::max)(page.capacity, 1u));
                if (page.liveCount == 0u) {
                    page.state = ParticlePageState::Reserved;
                } else if (page.occupancyQ16 < (1u << 13)) {
                    page.state = ParticlePageState::Sparse;
                } else {
                    page.state = ParticlePageState::Active;
                }
            }
        }

        if (needsSimulation) {
            TransitionBuffer(nativeCommandList, sharedHotBuffer->GetNativeResource(), particleResources.sharedArena.billboardHotState, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
            TransitionBuffer(nativeCommandList, sharedWarmBuffer->GetNativeResource(), particleResources.sharedArena.billboardWarmState, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
            TransitionBuffer(nativeCommandList, sharedColdBuffer->GetNativeResource(), particleResources.sharedArena.billboardColdState, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
            TransitionBuffer(nativeCommandList, sharedHeaderBuffer->GetNativeResource(), particleResources.sharedArena.billboardHeaderState, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
            TransitionBuffer(nativeCommandList, sharedMeshAttribHotBuffer->GetNativeResource(), particleResources.sharedArena.meshAttribHotState, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
            TransitionBuffer(nativeCommandList, sharedRibbonHistoryBuffer->GetNativeResource(), particleResources.sharedArena.ribbonHistoryState, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
            TransitionBuffer(nativeCommandList, sharedDeadListBuffer->GetNativeResource(), particleResources.sharedArena.deadListState, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
            TransitionBuffer(nativeCommandList, counterBuffer->GetNativeResource(), runtimeBuffers->counterState, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
            TransitionBuffer(nativeCommandList, sharedPageAliveCountBuffer->GetNativeResource(), particleResources.sharedArena.pageAliveCountState, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

            if (!runtimeBuffers->initialized) {
                // Initialize: zero all SoA streams, fill dead stack
                const auto initConstants = makeSimulationConstants(*packet, *runtimeBuffers, 0.0f, particleLifetime, runtimeBuffers->capacity);
                nativeCommandList->SetPipelineState(particleResources.initializePipelineState.Get());
                bindSimulationResources(initConstants, 0ull, hotGpuVa, warmGpuVa, coldGpuVa, headerGpuVa, deadListGpuVa, counterGpuVa, ribbonHistoryGpuVa, 0ull, meshAttribHotGpuVa);
                nativeCommandList->Dispatch((std::min)(runtimeBuffers->capacity, kMaxSingleDispatchParticles) / 64u + 1u, 1u, 1u);
                AddUavBarrier(nativeCommandList, sharedHotBuffer->GetNativeResource());
                AddUavBarrier(nativeCommandList, sharedWarmBuffer->GetNativeResource());
                AddUavBarrier(nativeCommandList, sharedColdBuffer->GetNativeResource());
                AddUavBarrier(nativeCommandList, sharedHeaderBuffer->GetNativeResource());
                AddUavBarrier(nativeCommandList, sharedRibbonHistoryBuffer->GetNativeResource());
                AddUavBarrier(nativeCommandList, sharedDeadListBuffer->GetNativeResource());
                AddUavBarrier(nativeCommandList, counterBuffer->GetNativeResource());
                runtimeBuffers->initialized = true;
            }

            // Reset counters + zero page alive counts BEFORE Emit/Update write to them
            // ResetCounters shader: u0=Counter, u1=PageAliveCount; totalPages via gTiming.w
            const auto resetConstants = makeSimulationConstants(*packet, *runtimeBuffers, 0.0f, particleLifetime, particleResources.sharedArena.totalPages);
            nativeCommandList->SetPipelineState(particleResources.resetCountersPipelineState.Get());
            bindSimulationResources(resetConstants, 0ull, counterGpuVa, pageAliveCountGpuVa, 0ull, 0ull, 0ull, 0ull, 0ull, 0ull, 0ull);
            nativeCommandList->Dispatch((particleResources.sharedArena.totalPages + 63u) / 64u, 1u, 1u);
            AddUavBarrier(nativeCommandList, counterBuffer->GetNativeResource());
            AddUavBarrier(nativeCommandList, sharedPageAliveCountBuffer->GetNativeResource());

            if (emitCount > 0u) {
                // Emit: write to Hot/Warm/Cold/Header, pop from dead stack, side-write PageAliveCount
                const auto emitConstants = makeSimulationConstants(*packet, *runtimeBuffers, simulationDeltaTime, particleLifetime, emitCount);
                nativeCommandList->SetPipelineState(particleResources.emitPipelineState.Get());
                bindSimulationResources(emitConstants, 0ull, hotGpuVa, warmGpuVa, coldGpuVa, headerGpuVa, deadListGpuVa, counterGpuVa, ribbonHistoryGpuVa, pageAliveCountGpuVa, meshAttribHotGpuVa);
                nativeCommandList->Dispatch((emitCount + 63u) / 64u, 1u, 1u);
                AddUavBarrier(nativeCommandList, sharedHotBuffer->GetNativeResource());
                AddUavBarrier(nativeCommandList, sharedWarmBuffer->GetNativeResource());
                AddUavBarrier(nativeCommandList, sharedColdBuffer->GetNativeResource());
                AddUavBarrier(nativeCommandList, sharedHeaderBuffer->GetNativeResource());
                AddUavBarrier(nativeCommandList, sharedRibbonHistoryBuffer->GetNativeResource());
                AddUavBarrier(nativeCommandList, sharedDeadListBuffer->GetNativeResource());
                AddUavBarrier(nativeCommandList, counterBuffer->GetNativeResource());
                AddUavBarrier(nativeCommandList, sharedPageAliveCountBuffer->GetNativeResource());
            }

            // Update: use readback if available, otherwise CPU estimate
            const uint32_t updateDispatchCount = (std::min)(estimatedAliveBase, 65535u * 64u);
            if (updateDispatchCount > 0u && simulationDeltaTime > 0.0f) {
                // Update: read Hot+Cold, write Hot+Warm+Header, side-write PageAliveCount
                const auto updateConstants = makeSimulationConstants(*packet, *runtimeBuffers, simulationDeltaTime, particleLifetime, updateDispatchCount);
                nativeCommandList->SetPipelineState(particleResources.updatePipelineState.Get());
                // t0=AliveList, u0=Hot, u1=Warm, u2=Cold, u3=Header, u4=Dead, u5=Counter, u6=Ribbon, u7=PageAliveCount
                bindSimulationResources(updateConstants, aliveListGpuVa, hotGpuVa, warmGpuVa, coldGpuVa, headerGpuVa, deadListGpuVa, counterGpuVa, ribbonHistoryGpuVa, pageAliveCountGpuVa, meshAttribHotGpuVa);
                nativeCommandList->Dispatch((updateDispatchCount + 63u) / 64u, 1u, 1u);
                AddUavBarrier(nativeCommandList, sharedHotBuffer->GetNativeResource());
                AddUavBarrier(nativeCommandList, sharedWarmBuffer->GetNativeResource());
                AddUavBarrier(nativeCommandList, sharedHeaderBuffer->GetNativeResource());
                AddUavBarrier(nativeCommandList, sharedRibbonHistoryBuffer->GetNativeResource());
                AddUavBarrier(nativeCommandList, sharedDeadListBuffer->GetNativeResource());
                AddUavBarrier(nativeCommandList, counterBuffer->GetNativeResource());
                AddUavBarrier(nativeCommandList, sharedPageAliveCountBuffer->GetNativeResource());
            }

            // PrefixSum on g_PageAliveCount -> g_PageAliveOffset
            // PrefixSum shader: u0=PageAliveCount, u1=PageAliveOffset, u2=Counter; totalPages via gTiming.w
            TransitionBuffer(nativeCommandList, sharedPageAliveOffsetBuffer->GetNativeResource(), particleResources.sharedArena.pageAliveOffsetState, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
            const auto prefixConstants = makeSimulationConstants(*packet, *runtimeBuffers, 0.0f, particleLifetime, particleResources.sharedArena.totalPages);
            nativeCommandList->SetPipelineState(particleResources.prefixSumPipelineState.Get());
            bindSimulationResources(prefixConstants, 0ull, pageAliveCountGpuVa, pageAliveOffsetGpuVa, counterGpuVa, 0ull, 0ull, 0ull, 0ull, 0ull, 0ull);
            nativeCommandList->Dispatch(1u, 1u, 1u);
            AddUavBarrier(nativeCommandList, sharedPageAliveOffsetBuffer->GetNativeResource());
            AddUavBarrier(nativeCommandList, counterBuffer->GetNativeResource());

            // ScatterAlive: per-page scatter -> g_AliveList
            // ScatterAlive shader: u0=AliveList, u1=PageAliveOffset, u2=Header
            TransitionBuffer(nativeCommandList, sharedAliveListBuffer->GetNativeResource(), particleResources.sharedArena.aliveListState, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
            const auto scatterConstants = makeSimulationConstants(*packet, *runtimeBuffers, 0.0f, particleLifetime, runtimeBuffers->pageCount);
            nativeCommandList->SetPipelineState(particleResources.scatterAlivePipelineState.Get());
            bindSimulationResources(scatterConstants, 0ull, aliveListGpuVa, pageAliveOffsetGpuVa, headerGpuVa, 0ull, 0ull, 0ull, 0ull, 0ull, 0ull);
            nativeCommandList->Dispatch(runtimeBuffers->pageCount, 1u, 1u);
            AddUavBarrier(nativeCommandList, sharedAliveListBuffer->GetNativeResource());

            // BuildDrawArgs: read alive count from counter, write indirect args
            // BuildDrawArgs shader: u0=IndirectArgs, u1=Counter
            TransitionBuffer(nativeCommandList, indirectArgsBuffer->GetNativeResource(), runtimeBuffers->indirectArgsState, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
            nativeCommandList->SetPipelineState(particleResources.buildDrawArgsPipelineState.Get());
            bindSimulationResources(resetConstants, 0ull, indirectArgsBuffer->GetGPUVirtualAddress(), counterGpuVa, 0ull, 0ull, 0ull, 0ull, 0ull, 0ull, 0ull);
            nativeCommandList->Dispatch(1u, 1u, 1u);
            AddUavBarrier(nativeCommandList, indirectArgsBuffer->GetNativeResource());

            // BuildBins: 2-stage group-local binning on alive list
            if (sharedBinCounterBuffer && sharedBinIndexBuffer) {
                TransitionBuffer(nativeCommandList, sharedBinCounterBuffer->GetNativeResource(), particleResources.sharedArena.binCounterState, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
                TransitionBuffer(nativeCommandList, sharedBinIndexBuffer->GetNativeResource(), particleResources.sharedArena.binIndexState, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
                // Bin counters are zeroed by BuildBinArgs at end of each frame
                nativeCommandList->SetComputeRootSignature(particleResources.binRootSignature.Get());
                nativeCommandList->SetPipelineState(particleResources.buildBinsPipelineState.Get());
                // [0] SRV t0=AliveList, [1] SRV t1=Warm, [2] SRV t2=Header
                // [3] UAV u0=BinIndex, [4] UAV u1=BinCounter, [5] UAV u2=CounterBuffer
                nativeCommandList->SetComputeRootShaderResourceView(0, aliveListGpuVa);
                nativeCommandList->SetComputeRootShaderResourceView(1, warmGpuVa);
                nativeCommandList->SetComputeRootShaderResourceView(2, headerGpuVa);
                nativeCommandList->SetComputeRootUnorderedAccessView(3, binIndexGpuVa);
                nativeCommandList->SetComputeRootUnorderedAccessView(4, binCounterGpuVa);
                nativeCommandList->SetComputeRootUnorderedAccessView(5, counterGpuVa);
                const uint32_t estimatedAlive = estimatedPostEmitCount;
                nativeCommandList->Dispatch((estimatedAlive + 255u) / 256u, 1u, 1u);
                AddUavBarrier(nativeCommandList, sharedBinCounterBuffer->GetNativeResource());
                AddUavBarrier(nativeCommandList, sharedBinIndexBuffer->GetNativeResource());

                // BuildBinArgs: per-bin indirect args from bin counters
                TransitionBuffer(nativeCommandList, sharedBinOffsetBuffer->GetNativeResource(), particleResources.sharedArena.binOffsetState, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
                TransitionBuffer(nativeCommandList, sharedBinIndirectArgsBuffer->GetNativeResource(), particleResources.sharedArena.binIndirectArgsState, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
                nativeCommandList->SetPipelineState(particleResources.buildBinArgsPipelineState.Get());
                // Reuse bin root sig: [3] u0=BinCounter(in), [4] u1=IndirectArgs(out), [5] u2=BinOffset(out)
                nativeCommandList->SetComputeRootUnorderedAccessView(3, binCounterGpuVa);
                nativeCommandList->SetComputeRootUnorderedAccessView(4, binIndirectArgsGpuVa);
                nativeCommandList->SetComputeRootUnorderedAccessView(5, binOffsetGpuVa);
                nativeCommandList->Dispatch(1u, 1u, 1u);
                AddUavBarrier(nativeCommandList, sharedBinIndirectArgsBuffer->GetNativeResource());
                AddUavBarrier(nativeCommandList, sharedBinOffsetBuffer->GetNativeResource());

                // Restore simulation root sig for subsequent emitters
                nativeCommandList->SetComputeRootSignature(particleResources.simulationRootSignature.Get());
            }

            // CoarseDepthBin: 2-stage depth binning for alpha/premul particles
            if (sharedDepthBinCounterBuffer && sharedDepthBinIndexBuffer && sharedDepthBinIndirectArgsBuffer &&
                particleResources.coarseDepthRootSignature && particleResources.coarseDepthPipelineState) {
                TransitionBuffer(nativeCommandList, sharedDepthBinCounterBuffer->GetNativeResource(), particleResources.sharedArena.depthBinCounterState, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
                TransitionBuffer(nativeCommandList, sharedDepthBinIndexBuffer->GetNativeResource(), particleResources.sharedArena.depthBinIndexState, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

                CoarseDepthConstants depthConstants{};
                DirectX::XMStoreFloat4x4(&depthConstants.viewMatrix, DirectX::XMLoadFloat4x4(&rc.viewMatrix));
                depthConstants.nearClip = rc.nearZ;
                depthConstants.farClip = rc.farZ;
                depthConstants.aliveCount = estimatedPostEmitCount;
                const auto depthCbAllocation = dx12CommandList->AllocateDynamicConstantBuffer(&depthConstants, static_cast<uint32_t>(sizeof(depthConstants)));

                nativeCommandList->SetComputeRootSignature(particleResources.coarseDepthRootSignature.Get());
                nativeCommandList->SetPipelineState(particleResources.coarseDepthPipelineState.Get());
                nativeCommandList->SetComputeRootConstantBufferView(0, depthCbAllocation.gpuVA);
                nativeCommandList->SetComputeRootShaderResourceView(1, aliveListGpuVa);
                nativeCommandList->SetComputeRootShaderResourceView(2, hotGpuVa);
                nativeCommandList->SetComputeRootUnorderedAccessView(3, depthBinIndexGpuVa);
                nativeCommandList->SetComputeRootUnorderedAccessView(4, depthBinCounterGpuVa);
                nativeCommandList->SetComputeRootUnorderedAccessView(5, counterGpuVa);
                nativeCommandList->Dispatch((estimatedPostEmitCount + 255u) / 256u, 1u, 1u);
                AddUavBarrier(nativeCommandList, sharedDepthBinCounterBuffer->GetNativeResource());
                AddUavBarrier(nativeCommandList, sharedDepthBinIndexBuffer->GetNativeResource());

                // DepthBinArgs
                TransitionBuffer(nativeCommandList, sharedDepthBinIndirectArgsBuffer->GetNativeResource(), particleResources.sharedArena.depthBinIndirectArgsState, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
                nativeCommandList->SetPipelineState(particleResources.depthBinArgsPipelineState.Get());
                nativeCommandList->SetComputeRootUnorderedAccessView(3, depthBinCounterGpuVa);
                nativeCommandList->SetComputeRootUnorderedAccessView(4, depthBinIndirectArgsGpuVa);
                nativeCommandList->Dispatch(1u, 1u, 1u);
                AddUavBarrier(nativeCommandList, sharedDepthBinIndirectArgsBuffer->GetNativeResource());

                nativeCommandList->SetComputeRootSignature(particleResources.simulationRootSignature.Get());
            }

            // Readback throttling: skip readback if within throttle interval
            const bool shouldReadback = (particleResources.frameCounter - runtimeBuffers->lastReadbackFrame) >= kReadbackThrottleInterval;
            if (shouldReadback) {
                TransitionBuffer(nativeCommandList, counterBuffer->GetNativeResource(), runtimeBuffers->counterState, D3D12_RESOURCE_STATE_COPY_SOURCE);
                QueueCounterReadback(nativeCommandList, *runtimeBuffers, counterBuffer, particleResources.frameCounter);
                runtimeBuffers->lastReadbackFrame = particleResources.frameCounter;
            }
        }

        runtimeBuffers->lastSimTime = currentTime;
        runtimeBuffers->lastSeed = seed;
        runtimeBuffers->lastEstimatedAlive = estimatedPostEmitCount;

        if (drawCount == 0u) {
            continue;
        }

        TransitionBuffer(nativeCommandList, sharedHotBuffer->GetNativeResource(), particleResources.sharedArena.billboardHotState, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
        TransitionBuffer(nativeCommandList, sharedWarmBuffer->GetNativeResource(), particleResources.sharedArena.billboardWarmState, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
        TransitionBuffer(nativeCommandList, sharedHeaderBuffer->GetNativeResource(), particleResources.sharedArena.billboardHeaderState, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
        TransitionBuffer(nativeCommandList, sharedAliveListBuffer->GetNativeResource(), particleResources.sharedArena.aliveListState, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
        TransitionBuffer(nativeCommandList, sharedRibbonHistoryBuffer->GetNativeResource(), particleResources.sharedArena.ribbonHistoryState, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
        TransitionBuffer(nativeCommandList, indirectArgsBuffer->GetNativeResource(), runtimeBuffers->indirectArgsState, D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT);
        if (sharedBinIndirectArgsBuffer) {
            TransitionBuffer(nativeCommandList, sharedBinIndirectArgsBuffer->GetNativeResource(), particleResources.sharedArena.binIndirectArgsState, D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT);
            TransitionBuffer(nativeCommandList, sharedBinIndexBuffer->GetNativeResource(), particleResources.sharedArena.binIndexState, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
        }
        if (sharedDepthBinIndirectArgsBuffer) {
            TransitionBuffer(nativeCommandList, sharedDepthBinIndirectArgsBuffer->GetNativeResource(), particleResources.sharedArena.depthBinIndirectArgsState, D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT);
            TransitionBuffer(nativeCommandList, sharedDepthBinIndexBuffer->GetNativeResource(), particleResources.sharedArena.depthBinIndexState, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
        }

        if (packet->drawMode == EffectParticleDrawMode::Mesh) {
            meshDrawEntries.push_back({ packet, aliveListGpuVa, hotGpuVa, warmGpuVa, headerGpuVa, meshAttribHotGpuVa, indirectArgsBuffer, drawCount });
        } else if (packet->drawMode == EffectParticleDrawMode::Ribbon) {
            ribbonDrawEntries.push_back({ packet, aliveListGpuVa, hotGpuVa, warmGpuVa, ribbonHistoryGpuVa, indirectArgsBuffer, packet->blendMode });
        } else {
            billboardDrawEntries.push_back({ packet, aliveListGpuVa, hotGpuVa, warmGpuVa, indirectArgsBuffer, sharedBinIndirectArgsBuffer, binIndexGpuVa, sharedDepthBinIndirectArgsBuffer, depthBinIndexGpuVa, packet->blendMode });
        }
    }

    PARTICLE_LOG("[EffectParticle] drawEntries billboard=%zu ribbon=%zu mesh=%zu",
        billboardDrawEntries.size(), ribbonDrawEntries.size(), meshDrawEntries.size());

    if (billboardDrawEntries.empty() && ribbonDrawEntries.empty() && meshDrawEntries.empty()) {
        return;
    }

    rc.commandList->TransitionBarrier(rtScene, ResourceState::RenderTarget);
    rc.commandList->TransitionBarrier(dsReal, ResourceState::DepthRead);
    dx12CommandList->FlushResourceBarriers();
    rc.commandList->SetRenderTarget(rtScene, dsReal);
    rc.mainRenderTarget = rtScene;
    rc.mainDepthStencil = dsReal;
    rc.mainViewport = RhiViewport(0.0f, 0.0f, static_cast<float>(rtScene->GetWidth()), static_cast<float>(rtScene->GetHeight()));
    rc.commandList->SetViewport(rc.mainViewport);

    EffectParticleSceneConstants sceneConstants{};
    FillSceneConstants(sceneConstants, rc);
    const auto sceneAllocation = dx12CommandList->AllocateDynamicConstantBuffer(&sceneConstants, static_cast<uint32_t>(sizeof(sceneConstants)));

    auto* d3dDevice = dx12Device->GetDevice();
    if (!d3dDevice) {
        return;
    }

    auto* depthTexture = dynamic_cast<DX12Texture*>(dsReal);
    const bool hasDepthSrv = depthTexture && depthTexture->HasSRV();
    const D3D12_CPU_DESCRIPTOR_HANDLE depthSrvHandle = hasDepthSrv ? depthTexture->GetSRV() : D3D12_CPU_DESCRIPTOR_HANDLE{};

    if (!billboardDrawEntries.empty()) {
        nativeCommandList->SetGraphicsRootSignature(particleResources.billboardRootSignature.Get());
        nativeCommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_POINTLIST);
        nativeCommandList->SetGraphicsRootConstantBufferView(0, sceneAllocation.gpuVA);
        EffectParticleBlendMode currentBillboardBlend = EffectParticleBlendMode::EnumCount; // force first set

        for (const auto& drawEntry : billboardDrawEntries) {
            if (drawEntry.blendMode != currentBillboardBlend) {
                currentBillboardBlend = drawEntry.blendMode;
                int blendIdx = static_cast<int>(currentBillboardBlend);
                if (blendIdx >= 0 && blendIdx < static_cast<int>(EffectParticleBlendMode::EnumCount) && particleResources.billboardBlendPSOs[blendIdx]) {
                    nativeCommandList->SetPipelineState(particleResources.billboardBlendPSOs[blendIdx].Get());
                } else {
                    nativeCommandList->SetPipelineState(particleResources.billboardPipelineState.Get());
                }
            }
            auto* texture = ResolveParticleTexture(particleResources, *drawEntry.packet);
            if (!texture) {
                continue;
            }

            EffectParticleRenderConstants renderConstants{};
            renderConstants.enableVelocityStretch = 1;
            renderConstants.velocityStretchScale = (drawEntry.packet->drawMode == EffectParticleDrawMode::Ribbon) ? (std::max)(drawEntry.packet->ribbonVelocityStretch, 0.18f) : 0.08f;
            renderConstants.velocityStretchMaxAspect = (drawEntry.packet->drawMode == EffectParticleDrawMode::Ribbon) ? 14.0f : 4.0f;
            renderConstants.velocityStretchMinSpeed = (drawEntry.packet->drawMode == EffectParticleDrawMode::Ribbon) ? 0.0f : 0.05f;
            renderConstants.globalAlpha = 1.0f;
            renderConstants.curlNoiseStrength = drawEntry.packet->softParticleEnabled ? (std::max)(drawEntry.packet->softParticleScale, 0.0f) : 0.0f;
            const auto renderAllocation = dx12CommandList->AllocateDynamicConstantBuffer(&renderConstants, static_cast<uint32_t>(sizeof(renderConstants)));

            D3D12_CPU_DESCRIPTOR_HANDLE spriteHandle = particleResources.textureHeapCpu;
            D3D12_CPU_DESCRIPTOR_HANDLE depthHandle = spriteHandle;
            depthHandle.ptr += static_cast<SIZE_T>(particleResources.textureHeapDescriptorSize);
            d3dDevice->CopyDescriptorsSimple(1, spriteHandle, texture->GetSRV(), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
            d3dDevice->CopyDescriptorsSimple(1, depthHandle, hasDepthSrv ? depthSrvHandle : texture->GetSRV(), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
            nativeCommandList->SetGraphicsRootShaderResourceView(2, drawEntry.billboardHotGpuVa);
            nativeCommandList->SetGraphicsRootShaderResourceView(3, drawEntry.billboardWarmGpuVa);
            nativeCommandList->SetGraphicsRootConstantBufferView(4, renderAllocation.gpuVA);
            nativeCommandList->SetGraphicsRootDescriptorTable(5, particleResources.textureHeapGpu);

            const bool useDepthBins = drawEntry.depthBinIndirectArgsBuffer &&
                drawEntry.depthBinIndexGpuVa != 0ull &&
                (drawEntry.packet->sortMode == EffectParticleSortMode::BackToFront ||
                 drawEntry.packet->sortMode == EffectParticleSortMode::FrontToBack);

            if (useDepthBins) {
                // CoarseDepthBin: far-to-near (or near-to-far) per-depth-bin ExecuteIndirect
                nativeCommandList->SetGraphicsRootShaderResourceView(1, drawEntry.depthBinIndexGpuVa);
                constexpr uint32_t kDepthBins = 32u;
                const bool farToNear = (drawEntry.packet->sortMode == EffectParticleSortMode::BackToFront);
                for (uint32_t i = 0u; i < kDepthBins; ++i) {
                    const uint32_t bin = farToNear ? (kDepthBins - 1u - i) : i;
                    nativeCommandList->ExecuteIndirect(
                        particleResources.depthBinCommandSignature.Get(),
                        1u,
                        drawEntry.depthBinIndirectArgsBuffer->GetNativeResource(),
                        bin * 16u,  // byte offset to this bin's D3D12_DRAW_ARGUMENTS
                        nullptr,
                        0u);
                }
            } else if (drawEntry.binIndirectArgsBuffer && drawEntry.binIndexGpuVa != 0ull) {
                // Per-renderer-bin ExecuteIndirect (unsorted / additive)
                nativeCommandList->SetGraphicsRootShaderResourceView(1, drawEntry.binIndexGpuVa);
                constexpr uint32_t kMaxBins = 16u;
                nativeCommandList->ExecuteIndirect(
                    particleResources.binCommandSignature.Get(),
                    kMaxBins,
                    drawEntry.binIndirectArgsBuffer->GetNativeResource(),
                    0u,
                    nullptr,
                    0u);
            } else {
                // Fallback: single ExecuteIndirect from alive list
                nativeCommandList->SetGraphicsRootShaderResourceView(1, drawEntry.aliveListGpuVa);
                nativeCommandList->ExecuteIndirect(
                    particleResources.billboardCommandSignature.Get(),
                    1u,
                    drawEntry.indirectArgsBuffer->GetNativeResource(),
                    0u,
                    nullptr,
                    0u);
            }
        }
    }

    if (!ribbonDrawEntries.empty()) {
        nativeCommandList->SetGraphicsRootSignature(particleResources.ribbonRootSignature.Get());
        nativeCommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_POINTLIST);
        nativeCommandList->SetGraphicsRootConstantBufferView(0, sceneAllocation.gpuVA);
        EffectParticleBlendMode currentRibbonBlend = EffectParticleBlendMode::EnumCount;

        for (const auto& drawEntry : ribbonDrawEntries) {
            if (drawEntry.blendMode != currentRibbonBlend) {
                currentRibbonBlend = drawEntry.blendMode;
                int blendIdx = static_cast<int>(currentRibbonBlend);
                if (blendIdx >= 0 && blendIdx < static_cast<int>(EffectParticleBlendMode::EnumCount) && particleResources.ribbonBlendPSOs[blendIdx]) {
                    nativeCommandList->SetPipelineState(particleResources.ribbonBlendPSOs[blendIdx].Get());
                } else {
                    nativeCommandList->SetPipelineState(particleResources.ribbonPipelineState.Get());
                }
            }
            auto* texture = ResolveParticleTexture(particleResources, *drawEntry.packet);
            if (!texture) {
                continue;
            }

            EffectParticleRenderConstants renderConstants{};
            renderConstants.enableVelocityStretch = 0;
            renderConstants.velocityStretchScale = 0.0f;
            renderConstants.velocityStretchMaxAspect = 1.0f;
            renderConstants.velocityStretchMinSpeed = 0.0f;
            renderConstants.globalAlpha = 1.0f;
            renderConstants.curlNoiseStrength = drawEntry.packet->softParticleEnabled ? (std::max)(drawEntry.packet->softParticleScale, 0.0f) : 0.0f;
            const auto renderAllocation = dx12CommandList->AllocateDynamicConstantBuffer(&renderConstants, static_cast<uint32_t>(sizeof(renderConstants)));

            D3D12_CPU_DESCRIPTOR_HANDLE spriteHandle = particleResources.textureHeapCpu;
            D3D12_CPU_DESCRIPTOR_HANDLE depthHandle = OffsetCpuHandle(spriteHandle, particleResources.textureHeapDescriptorSize, 1u);
            d3dDevice->CopyDescriptorsSimple(1, spriteHandle, texture->GetSRV(), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
            d3dDevice->CopyDescriptorsSimple(1, depthHandle, hasDepthSrv ? depthSrvHandle : texture->GetSRV(), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

            nativeCommandList->SetGraphicsRootShaderResourceView(1, drawEntry.aliveListGpuVa);
            nativeCommandList->SetGraphicsRootShaderResourceView(2, drawEntry.particleDataGpuVa);
            nativeCommandList->SetGraphicsRootShaderResourceView(3, drawEntry.warmGpuVa);
            nativeCommandList->SetGraphicsRootShaderResourceView(4, drawEntry.ribbonHistoryGpuVa);
            nativeCommandList->SetGraphicsRootConstantBufferView(5, renderAllocation.gpuVA);
            nativeCommandList->SetGraphicsRootDescriptorTable(6, particleResources.textureHeapGpu);
            if (drawEntry.indirectArgsBuffer) {
                nativeCommandList->ExecuteIndirect(
                    particleResources.billboardCommandSignature.Get(),
                    1u,
                    drawEntry.indirectArgsBuffer->GetNativeResource(),
                    0u,
                    nullptr,
                    0u);
            }
        }
    }

    if (!meshDrawEntries.empty()) {
        nativeCommandList->SetGraphicsRootSignature(particleResources.meshRootSignature.Get());
        nativeCommandList->SetPipelineState(particleResources.meshPipelineState.Get());
        nativeCommandList->SetGraphicsRootConstantBufferView(0, sceneAllocation.gpuVA);

        EffectParticleRenderConstants renderConstants{};
        renderConstants.enableVelocityStretch = 0;
        renderConstants.velocityStretchMaxAspect = 1.0f;
        renderConstants.globalAlpha = 1.0f;
        const auto renderAllocation = dx12CommandList->AllocateDynamicConstantBuffer(&renderConstants, static_cast<uint32_t>(sizeof(renderConstants)));
        nativeCommandList->SetGraphicsRootConstantBufferView(1, renderAllocation.gpuVA);

        for (const auto& drawEntry : meshDrawEntries) {
            auto* texture = ResolveParticleTexture(particleResources, *drawEntry.packet);
            if (!texture || !drawEntry.packet->modelResource) {
                continue;
            }

            d3dDevice->CopyDescriptorsSimple(1, particleResources.textureHeapCpu, texture->GetSRV(), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
            // SoA mesh root sig: [2]=AliveList(t0) [3]=Hot(t1) [4]=Warm(t2) [5]=Header(t3) [6]=MeshAttribHot(t4) [7]=table t5 color_map
            nativeCommandList->SetGraphicsRootShaderResourceView(2, drawEntry.aliveListGpuVa);
            nativeCommandList->SetGraphicsRootShaderResourceView(3, drawEntry.hotGpuVa);
            nativeCommandList->SetGraphicsRootShaderResourceView(4, drawEntry.warmGpuVa);
            nativeCommandList->SetGraphicsRootShaderResourceView(5, drawEntry.headerGpuVa);
            nativeCommandList->SetGraphicsRootShaderResourceView(6, drawEntry.meshAttribHotGpuVa);
            nativeCommandList->SetGraphicsRootDescriptorTable(7, particleResources.textureHeapGpu);

            for (const auto& meshResource : drawEntry.packet->modelResource->GetMeshResources()) {
                auto* vertexBuffer = dynamic_cast<DX12Buffer*>(meshResource.vertexBuffer.get());
                auto* indexBuffer = dynamic_cast<DX12Buffer*>(meshResource.indexBuffer.get());
                if (!vertexBuffer || !indexBuffer || !vertexBuffer->IsValid() || !indexBuffer->IsValid() || meshResource.indexCount == 0) {
                    continue;
                }

                D3D12_VERTEX_BUFFER_VIEW vbView = {};
                vbView.BufferLocation = vertexBuffer->GetGPUVirtualAddress();
                vbView.SizeInBytes = vertexBuffer->GetSize();
                vbView.StrideInBytes = meshResource.vertexStride;

                D3D12_INDEX_BUFFER_VIEW ibView = {};
                ibView.BufferLocation = indexBuffer->GetGPUVirtualAddress();
                ibView.SizeInBytes = indexBuffer->GetSize();
                ibView.Format = DXGI_FORMAT_R32_UINT;

                nativeCommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
                nativeCommandList->IASetVertexBuffers(0, 1, &vbView);
                nativeCommandList->IASetIndexBuffer(&ibView);
                nativeCommandList->DrawIndexedInstanced(meshResource.indexCount, drawEntry.drawCount, 0u, 0, 0u);
            }
        }
    }

    // --- Trail rendering ---
    if (!queue.trailPackets.empty() && particleResources.trailPipelineState)
    {
        nativeCommandList->SetGraphicsRootSignature(particleResources.trailRootSignature.Get());
        nativeCommandList->SetPipelineState(particleResources.trailPipelineState.Get());
        nativeCommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        nativeCommandList->SetGraphicsRootConstantBufferView(0, sceneAllocation.gpuVA);

        for (const auto& trail : queue.trailPackets)
        {
            if (trail.vertices.empty() || trail.indices.empty()) continue;

            const uint32_t vbSize = static_cast<uint32_t>(trail.vertices.size() * sizeof(TrailVertex));
            const uint32_t ibSize = static_cast<uint32_t>(trail.indices.size() * sizeof(uint32_t));

            const auto vbAlloc = dx12CommandList->AllocateDynamicConstantBuffer(trail.vertices.data(), vbSize);
            const auto ibAlloc = dx12CommandList->AllocateDynamicConstantBuffer(trail.indices.data(), ibSize);

            D3D12_VERTEX_BUFFER_VIEW vbView = {};
            vbView.BufferLocation = vbAlloc.gpuVA;
            vbView.SizeInBytes = vbSize;
            vbView.StrideInBytes = sizeof(TrailVertex);

            D3D12_INDEX_BUFFER_VIEW ibView = {};
            ibView.BufferLocation = ibAlloc.gpuVA;
            ibView.SizeInBytes = ibSize;
            ibView.Format = DXGI_FORMAT_R32_UINT;

            nativeCommandList->IASetVertexBuffers(0, 1, &vbView);
            nativeCommandList->IASetIndexBuffer(&ibView);
            nativeCommandList->DrawIndexedInstanced(static_cast<UINT>(trail.indices.size()), 1, 0, 0, 0);
        }
    }

    dx12CommandList->RestoreDescriptorHeap();
    rc.commandList->SetRenderTarget(nullptr, nullptr);

    for (auto it = particleResources.runtimeAllocations.begin(); it != particleResources.runtimeAllocations.end(); ) {
        if (particleResources.frameCounter > it->second.lastSeenFrame &&
            (particleResources.frameCounter - it->second.lastSeenFrame) > 240u) {
            // Decrement per-renderer budget tracking before releasing
            uint32_t& slots = GetRendererAllocatedSlots(particleResources.sharedArena, it->second.rendererType);
            slots -= (std::min)(slots, it->second.capacity);
            ReleaseArenaPages(particleResources.sharedArena, it->second);
            it = particleResources.runtimeAllocations.erase(it);
        } else {
            ++it;
        }
    }
}
