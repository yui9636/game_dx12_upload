#pragma once
#include "IRenderPass.h"
#include "RenderGraph/FrameGraphTypes.h"
#include <memory>

class IShader;
class IPipelineState;
class IResourceFactory;

class FinalBlitPass : public IRenderPass
{
public:
    explicit FinalBlitPass(IResourceFactory* factory);
    ~FinalBlitPass() override = default;

    std::string GetName() const override { return "FinalBlitPass"; }
    bool HasSideEffects() const override { return true; }

    void Setup(FrameGraphBuilder& builder) override;
    void Execute(FrameGraphResources& resources, const RenderQueue& queue, RenderContext& rc) override;

    IPipelineState* GetPSO() const { return m_pso.get(); }

private:
    std::shared_ptr<IShader> m_vs;
    std::shared_ptr<IShader> m_ps;
    std::shared_ptr<IPipelineState> m_pso;
    ResourceHandle m_hSceneColor;
    ResourceHandle m_hDisplayColor;
};
