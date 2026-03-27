#pragma once
#include <memory>
#include <vector>
#include "../Graphics.h"
#include "RenderContext.h"
#include "../RenderPass/IRenderPass.h"
#include "RHI/ICommandList.h"
#include "RHI/DX12/DX12RootSignature.h"
#include <unordered_map>
#include <array>
#include <RenderGraph/FrameGraph.h>
#include "System/TaskSystem.h"
#include "Model/ModelRenderer.h"

class Registry;

class RenderPipeline
{
public:
    static constexpr int kMaxInFlight = 3;

    struct RenderViewContext
    {
        RenderContext::ViewState state;
        ITexture* sceneViewTexture = nullptr;
        ITexture* prevSceneTexture = nullptr;
        ITexture* displayTexture = nullptr;
        ITexture* sceneDepthTexture = nullptr;
        ITexture* debugGBuffer0 = nullptr;
        ITexture* debugGBuffer1 = nullptr;
        ITexture* debugGBuffer2 = nullptr;
        ITexture* debugDepth = nullptr;
    };

    RenderPipeline() = default;
    ~RenderPipeline() = default;
    void AddPass(std::shared_ptr<IRenderPass> pass) {
        m_passes.push_back(pass);
    }
    void ClearPasses() {
        m_passes.clear();
    }

    RenderContext BeginFrame(Registry& registry, FrameBuffer* targetBuffer = nullptr);

    void Execute(const RenderQueue& queue, RenderContext& rc);
    void ExecuteViews(const RenderQueue& queue, RenderContext& rc, const std::vector<RenderContext::ViewState>& views);
    void ExecuteViews(const RenderQueue& queue, RenderContext& rc, std::vector<RenderViewContext>& views);

    void EndFrame(RenderContext& rc);
    void SubmitFrame(RenderContext& rc);

    void BlitSceneToDisplay(RenderContext& rc);
    RenderViewContext BuildPrimaryViewContext(const RenderContext& rc, uint32_t panelWidth = 0, uint32_t panelHeight = 0) const;
    RenderContext::ViewState BuildPrimaryViewState(const RenderContext& rc, uint32_t panelWidth = 0, uint32_t panelHeight = 0) const;

private:
    struct ViewFrameBuffers
    {
        std::unique_ptr<FrameBuffer> scene;
        std::unique_ptr<FrameBuffer> prevScene;
        std::unique_ptr<FrameBuffer> display;
        uint32_t renderWidth = 0;
        uint32_t renderHeight = 0;
        uint32_t displayWidth = 0;
        uint32_t displayHeight = 0;
    };

    struct ViewHistoryState
    {
        DirectX::XMFLOAT4X4 prevViewProjection{};
        DirectX::XMFLOAT2 prevJitterOffset = { 0.0f, 0.0f };
        bool initialized = false;
        std::unique_ptr<FrameGraph> frameGraph;
        std::vector<std::shared_ptr<IRenderPass>> passes;
        std::shared_ptr<ShadowMap> shadowMap;
        std::unique_ptr<ModelRenderer> modelRenderer;
        std::shared_ptr<IBuffer> sceneBuffer;
        std::shared_ptr<IBuffer> shadowBuffer;
        std::shared_ptr<IBuffer> preparedInstanceBuffer;
        std::shared_ptr<IBuffer> preparedVisibleInstanceStructuredBuffer;
        std::shared_ptr<IBuffer> preparedIndirectArgumentBuffer;
        std::shared_ptr<IBuffer> preparedIndirectCommandMetadataBuffer;
        uint32_t preparedInstanceCapacity = 0;
        uint32_t preparedIndirectArgumentCapacity = 0;
        uint32_t preparedIndirectCommandMetadataCapacity = 0;
        ViewFrameBuffers frameBuffers;
    };

    void ExecuteView(const RenderQueue& queue, RenderContext& baseRc, RenderViewContext& viewContext);
    void EnsureViewFrameBuffers(ViewHistoryState& history, const RenderContext::ViewState& viewState, IResourceFactory* factory);
    void PruneInactiveViews(const std::vector<RenderViewContext>& views);

    std::vector<std::shared_ptr<IRenderPass>> m_passes;
    std::unordered_map<uint64_t, ViewHistoryState> m_viewHistory;

    std::unique_ptr<ICommandList> m_commandList;
    std::unique_ptr<DX12RootSignature> m_dx12RootSig;
    std::vector<IRenderPass*> m_graphPassScratch;
    std::vector<TaskSystem::TaskGraphNode> m_prepGraphScratch;
    std::array<std::vector<std::shared_ptr<DX12CommandList>>, kMaxInFlight> m_workerDx12CommandListPools;

    // Keep GPU buffers alive across frames to prevent use-after-free
    // (GPU may still be reading previous frame's buffers)
    struct FrameGpuResources {
        std::shared_ptr<IBuffer> instanceBuffer;
        std::shared_ptr<IBuffer> instanceStructuredBuffer;
        std::shared_ptr<IBuffer> drawArgsBuffer;
        std::shared_ptr<IBuffer> metadataBuffer;
    };
    FrameGpuResources m_inFlightResources[kMaxInFlight];
    std::vector<std::shared_ptr<DX12CommandList>> m_inFlightRecordedDx12Lists[kMaxInFlight];
    int m_inFlightIndex = 0;
    uint64_t m_frameSerial = 0;
};
