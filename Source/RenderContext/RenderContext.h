#pragma once
#include "RenderState.h"
#include "RenderQueue.h"
#include "Light/Light.h"
#include <string>
#include <functional>
#include <memory>
#include <DirectXMath.h>
#include "RHI/ICommandList.h"
#include "RHI/IBuffer.h"
#include "RHI/ITexture.h"
#include "IndirectDrawCommon.h"

class ShadowMap;
class Skybox;
class ModelResource;
class ModelRenderer;
class DX12RootSignature;
class DX12CommandList;
class IBuffer;


struct BloomData {
    float luminanceLowerEdge = 0.6f;
    float luminanceHigherEdge = 0.8f;
    float bloomIntensity = 1.0f;
    float gaussianSigma = 1.0f;
};

struct ColorFilterData {
    float exposure = 1.2f;
    float monoBlend = 0.0f;
    float hueShift = 0.0f;
    float flashAmount = 0.0f;
    float vignetteAmount = 0.0f;
    std::string lutPath;          // color-grading LUT texture path ("" = disabled)
    float lutAmount = 0.0f;       // LUT blend strength [0,1]
};

// One deferred decal extracted from a DecalComponent entity for the current frame.
struct DecalInstance {
    DirectX::XMFLOAT4X4 worldMatrix;                    // unit cube [-0.5,0.5] -> world
    DirectX::XMFLOAT4X4 invWorldMatrix;                 // world -> box-local
    ITexture* texture = nullptr;                        // decal albedo texture
    DirectX::XMFLOAT4 tintOpacity = { 1, 1, 1, 1 };     // rgb tint, a = opacity
    DirectX::XMFLOAT3 projectionAxis = { 0, 0, 1 };     // world-space decal forward (+Z)
    float angleFade = 0.25f;                            // surface-normal fade threshold
};


struct DepthOfFieldData
{
    bool  enable = false;
    float focusDistance = 10.0f;
    float focusRange = 5.0f;
    float bokehRadius = 4.0f;
};

struct MotionBlurData
{
    float intensity = 0.0f;
    float samples = 8.0f;
};

struct RenderEnvironment
{
    std::string skyboxPath = "";
    std::string diffuseIBLPath = "";
    std::string specularIBLPath = "";
};

struct UVScrollData
{
    DirectX::XMFLOAT2 uvScrollValue;
};

struct MaskData
{
    ID3D11ShaderResourceView* maskTexture;
    float dissolveThreshold;
    float edgeThreshold;
    DirectX::XMFLOAT4 edgeColor;
};

struct RadialBlurData
{
    float radius = 10.0f;
    int samplingCount = 10;
    DirectX::XMFLOAT2 center = { 0.5f, 0.5f };
    float mask_radius = 0;
};

struct GaussianFilterData {
    int kernelSize = 8;
    float deviation = 10.0f;
    DirectX::XMFLOAT2 textureSize;
};

static const int MaxKernelSize = 16;

struct RenderPipelineSettings
{
    bool enableShadow = true;
    bool enableSkybox = true;
    bool enablePostProcess = true;
    bool enableGrid = true;
    float clearColor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
};


struct RenderContext
{
    struct ViewState
    {
        uint64_t historyKey = 0;
        RhiViewport viewport;
        uint32_t renderWidth = 0;
        uint32_t renderHeight = 0;
        uint32_t displayWidth = 0;
        uint32_t displayHeight = 0;
        uint32_t panelWidth = 0;
        uint32_t panelHeight = 0;
        ITexture* mainRenderTarget = nullptr;
        ITexture* mainDepthStencil = nullptr;
        ITexture* sceneColorTexture = nullptr;
        ITexture* sceneDepthTexture = nullptr;
        ITexture* prevSceneTexture = nullptr;
        ITexture* displayColorTexture = nullptr;
        bool enableComputeCulling = true;
        bool enableAsyncCompute = true;
        bool enableGTAO = true;
        bool enableSSGI = true;
        bool enableVolumetricFog = true;
        bool enableSSR = true;
        bool enableDeferredLighting = true;
        bool enableSkybox = true;
        bool enablePostProcess = true;
        bool enableTemporalJitter = true;
        bool clearSceneColor = true;
        bool cameraPixelSnap = false;
        DirectX::XMFLOAT4 clearColor = { 0.0f, 0.0f, 0.0f, 1.0f };

        DirectX::XMFLOAT4X4 viewMatrix;
        DirectX::XMFLOAT4X4 projectionMatrix;
        DirectX::XMFLOAT4X4 viewProjectionUnjittered;
        DirectX::XMFLOAT4X4 prevViewProjectionMatrix;
        DirectX::XMFLOAT3 cameraPosition = { 0.0f, 0.0f, 0.0f };
        DirectX::XMFLOAT3 cameraDirection = { 0.0f, 0.0f, 1.0f };
        float fovY = 0.785f;
        float aspect = 1.0f;
        float nearZ = 0.1f;
        float farZ = 1000.0f;
        DirectX::XMFLOAT2 jitterOffset = { 0.0f, 0.0f };
        DirectX::XMFLOAT2 prevJitterOffset = { 0.0f, 0.0f };
    };

    struct PreparationMetrics
    {
        double sceneUploadMs = 0.0;
        double frameGraphSetupMs = 0.0;
        double frameGraphCompileMs = 0.0;
        double frameGraphExecuteMs = 0.0;
        double submitFrameMs = 0.0;
        double visibleExtractMs = 0.0;
        double instanceBuildMs = 0.0;
        double indirectBuildMs = 0.0;
        double asyncComputeSubmitMs = 0.0;
        double asyncComputeGpuMs = 0.0;
        float visibleInstanceHitRate = 0.0f;
        float averageInstancesPerVisibleBatch = 0.0f;
        uint32_t visibleBatchCount = 0;
        uint32_t visibleInstanceCount = 0;
        uint32_t preparedBatchCount = 0;
        uint32_t preparedIndirectCount = 0;
        uint32_t preparedSkinnedCount = 0;
        uint32_t instanceBufferReallocs = 0;
        uint32_t visibleStructuredBufferReallocs = 0;
        uint32_t indirectBufferReallocs = 0;
        uint32_t visibleScratchVectorGrowths = 0;
        uint32_t preparedInstanceVectorGrowths = 0;
        uint32_t preparedBatchVectorGrowths = 0;
        uint32_t indirectScratchVectorGrowths = 0;
        uint32_t drawArgsVectorGrowths = 0;
        uint32_t metadataVectorGrowths = 0;
        uint32_t skinnedCommandCount = 0;
        uint32_t nonSkinnedCommandCount = 0;
        uint32_t gpuDrivenCandidateBatchCount = 0;
        uint32_t gpuDrivenCandidateInstanceCount = 0;
        uint32_t gpuDrivenDispatchGroupCount = 0;
        uint32_t asyncComputeDispatchCount = 0;
        uint32_t asyncComputeFallbackCount = 0;
        uint32_t asyncComputeWaitCount = 0;
        float gpuDrivenDispatchReduction = 0.0f;
    };

    struct PreparedInstanceBatch
    {
        DrawBatchKey key;
        std::shared_ptr<ModelResource> modelResource;
        uint32_t firstInstance = 0;
        uint32_t instanceCount = 0;
    };

    struct PreparedIndirectCommand
    {
        DrawBatchKey key;
        std::shared_ptr<ModelResource> modelResource;
        uint32_t meshIndex = 0;
        uint32_t firstInstance = 0;
        uint32_t instanceCount = 0;
        uint32_t argumentOffsetBytes = 0;
        bool supportsInstancing = false;
    };

    using GpuDrivenCommandMetadata = CullCommandMeta;

    struct GpuDrivenDispatchGroup
    {
        DrawBatchKey key;
        std::shared_ptr<ModelResource> modelResource;
        uint32_t meshIndex = 0;
        uint32_t firstCommand = 0;
        uint32_t commandCount = 0;
        uint32_t firstArgumentOffsetBytes = 0;
        bool supportsInstancing = false;
    };

    bool HasPreparedOpaqueCommands() const
    {
        return !activeDrawCommands.empty() || !activeSkinnedCommands.empty();
    }
    ICommandList* commandList;
    const RenderState* renderState;
    DX12RootSignature* dx12RootSignature = nullptr;
    ModelRenderer* modelRendererOverride = nullptr;
    std::vector<std::shared_ptr<DX12CommandList>>* recordedDx12CommandLists = nullptr;
    std::vector<std::shared_ptr<DX12CommandList>>* workerDx12CommandListPool = nullptr;
    IBuffer* sceneConstantBufferOverride = nullptr;
    IBuffer* shadowConstantBufferOverride = nullptr;

    RhiViewport mainViewport;
    uint32_t renderWidth = 0;
    uint32_t renderHeight = 0;
    uint32_t displayWidth = 0;
    uint32_t displayHeight = 0;
    uint32_t panelWidth = 0;
    uint32_t panelHeight = 0;
    ITexture* mainRenderTarget = nullptr;
    ITexture* mainDepthStencil = nullptr;

    DirectionalLight        directionalLight;
    std::vector<PointLight> pointLights;
    std::vector<DecalInstance> decals;

    DirectX::XMFLOAT4X4 viewMatrix;
    DirectX::XMFLOAT4X4 projectionMatrix;
    DirectX::XMFLOAT4X4 viewProjectionUnjittered;
    DirectX::XMFLOAT4X4 prevViewProjectionMatrix;
    DirectX::XMFLOAT3   cameraPosition;
    DirectX::XMFLOAT3   cameraDirection;

    float fovY;
    float aspect;
    float nearZ;
    float farZ;
    DirectX::XMFLOAT2 jitterOffset = { 0.0f, 0.0f };
    DirectX::XMFLOAT2 prevJitterOffset = { 0.0f, 0.0f };

    const ShadowMap* shadowMap = nullptr;

    DirectX::XMFLOAT3 shadowColor = { 0.1f, 0.1f, 0.1f };

    ITexture* sceneColorTexture = nullptr;
    ITexture* sceneDepthTexture = nullptr;

    ITexture* reflectionProbeTexture = nullptr;

    ITexture* debugGBuffer0 = nullptr;
    ITexture* debugGBuffer1 = nullptr;
    ITexture* debugGBuffer2 = nullptr;
    ITexture* debugGBufferDepth = nullptr;

    RenderEnvironment environment;

    std::vector<InstanceBatch> visibleOpaqueInstanceBatches;
    std::vector<InstanceData> preparedInstanceData;
    std::shared_ptr<IBuffer> preparedInstanceBuffer;
    std::shared_ptr<IBuffer> preparedVisibleInstanceStructuredBuffer;
    uint32_t preparedInstanceStride = 0;
    uint32_t preparedInstanceCapacity = 0;
    uint32_t preparedVisibleInstanceCount = 0;
    std::shared_ptr<IBuffer> preparedIndirectArgumentBuffer;
    std::shared_ptr<IBuffer> preparedIndirectCommandMetadataBuffer;
    uint32_t preparedIndirectArgumentCapacity = 0;
    uint32_t preparedIndirectCommandMetadataCapacity = 0;
    std::vector<PreparedInstanceBatch> preparedOpaqueInstanceBatches;
    std::vector<PreparedIndirectCommand> preparedIndirectCommands;
    std::vector<PreparedIndirectCommand> preparedSkinnedCommands;
    std::vector<GpuDrivenDispatchGroup> gpuDrivenDispatchGroups;

    // 現在有効な描画状態。BuildIndirectCommandPass が設定し、ComputeCullingPass が上書きする。
    IBuffer*  activeInstanceBuffer   = nullptr;   // 頂点バッファ slot 1。
    uint32_t  activeInstanceStride   = INSTANCE_DATA_STRIDE;
    IBuffer*  activeDrawArgsBuffer   = nullptr;   // ExecuteIndirect 用 args。
    std::vector<IndirectDrawCommand> activeDrawCommands;
    std::vector<IndirectDrawCommand> activeSkinnedCommands;

    // ComputeCullingPass が設定する GPU culling 用状態。
    bool useGpuCulling = false;
    bool allowGpuDrivenCompute = true;
    bool allowAsyncCompute = true;
    bool allowParallelRecording = true;
    bool enableGTAO = true;
    bool enableSSGI = true;
    bool enableVolumetricFog = true;
    bool enableSSR = true;
    bool enableDeferredLighting = true;
    bool enableSkybox = true;
    bool enablePostProcess = true;
    bool enableTemporalJitter = true;
    bool clearSceneColor = true;
    bool cameraPixelSnap = false;
    DirectX::XMFLOAT4 clearColor = { 0.0f, 0.0f, 0.0f, 1.0f };
    IBuffer* activeCountBuffer = nullptr;      // multi-draw 用の count buffer。
    uint32_t activeCountBufferOffset = 0;
    uint32_t activeMaxDrawCount = 0;           // multi-draw で扱う command 数の上限。
    uint64_t pendingAsyncComputeFenceValue = 0;

    BloomData       bloomData;
    ColorFilterData colorFilterData;
    DepthOfFieldData dofData;       //
    MotionBlurData  motionBlurData;
float time = 0.0f;

    UVScrollData uvScrollData;
    MaskData maskData;
    RadialBlurData radialBlurData;
    GaussianFilterData gaussianFilterData;

    PreparationMetrics prepMetrics;

   
};
