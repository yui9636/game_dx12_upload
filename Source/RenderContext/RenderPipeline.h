#pragma once
#include <memory>
#include <vector>
#include "../Graphics.h"
#include "RenderContext.h"
#include "../RenderPass/IRenderPass.h"
#include "RHI/ICommandList.h"
#include "RHI/DX12/DX12RootSignature.h"
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

    void EndFrame(RenderContext& rc);
    void SubmitFrame(RenderContext& rc);

    // DX12: FrameGraph外で SceneColor → DisplayColor をトーンマップBlit
    void BlitSceneToDisplay(RenderContext& rc);

private:
    std::vector<std::shared_ptr<IRenderPass>> m_passes;

    DirectX::XMFLOAT4X4 m_prevViewProjection; 
    DirectX::XMFLOAT2 m_prevJitterOffset = { 0.0f, 0.0f };
    bool m_isFirstFrame = true;             

    std::unique_ptr<ICommandList> m_commandList;
    std::unique_ptr<DX12RootSignature> m_dx12RootSig;
    FrameGraph m_frameGraph;

};