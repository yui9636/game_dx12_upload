#pragma once
#include "RenderPass/IRenderPass.h"
#include "RenderGraph/FrameGraphTypes.h"
#include <memory>

class IShader;
class IPipelineState;
class IResourceFactory;

// GBuffer の normal / world position から screen-space ambient occlusion を作る pass。
class GTAOPass : public IRenderPass {
public:
    GTAOPass(IResourceFactory* factory);
    ~GTAOPass() override;

    std::string GetName() const override { return "GTAOPass"; }

    void Setup(FrameGraphBuilder& builder, const RenderContext& rc) override;
    void Execute(FrameGraphResources& resources, const RenderQueue& queue, RenderContext& rc) override;

private:
    // full-screen GTAO 用 shader / PSO。
    std::unique_ptr<IShader> m_vs;
    std::unique_ptr<IShader> m_ps;

    std::unique_ptr<IPipelineState> m_pso;
    ResourceHandle m_hGBuffer1; // 法線。
    ResourceHandle m_hGBuffer2; // WorldPos 用。
    ResourceHandle m_hGTAO;     // R8 の AO 出力。
};
