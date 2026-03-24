#pragma once
#include <memory>
#include <vector>
#include "../Graphics.h"
#include "RenderContext.h"
#include "../RenderPass/IRenderPass.h"
#include "RHI/ICommandList.h"
#include "RHI/DX12/DX12RootSignature.h"
#include <unordered_map>
#include <RenderGraph/FrameGraph.h>

class Registry;

class RenderPipeline
{
public:
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

    void EndFrame(RenderContext& rc);
    void SubmitFrame(RenderContext& rc);

    void BlitSceneToDisplay(RenderContext& rc);

private:
    struct ViewHistoryState
    {
        DirectX::XMFLOAT4X4 prevViewProjection{};
        DirectX::XMFLOAT2 prevJitterOffset = { 0.0f, 0.0f };
        bool initialized = false;
    };

    void ExecuteSingleView(const RenderQueue& queue, RenderContext& baseRc, const RenderContext::ViewState& viewState);
    RenderContext::ViewState BuildPrimaryViewState(const RenderContext& rc) const;

    std::vector<std::shared_ptr<IRenderPass>> m_passes;
    std::unordered_map<uint64_t, ViewHistoryState> m_viewHistory;

    std::unique_ptr<ICommandList> m_commandList;
    std::unique_ptr<DX12RootSignature> m_dx12RootSig;
    FrameGraph m_frameGraph;

    // Keep GPU buffers alive across frames to prevent use-after-free
    // (GPU may still be reading previous frame's buffers)
    static constexpr int kMaxInFlight = 3;
    struct FrameGpuResources {
        std::shared_ptr<IBuffer> instanceBuffer;
        std::shared_ptr<IBuffer> instanceStructuredBuffer;
        std::shared_ptr<IBuffer> drawArgsBuffer;
        std::shared_ptr<IBuffer> metadataBuffer;
    };
    FrameGpuResources m_inFlightResources[kMaxInFlight];
    int m_inFlightIndex = 0;
};
