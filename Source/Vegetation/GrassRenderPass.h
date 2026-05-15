#pragma once
#include "RenderPass/IRenderPass.h"
#include "RenderGraph/FrameGraphTypes.h"
#include "RHI/IPipelineState.h"
#include "RHI/IState.h"
#include "RHI/IShader.h"
#include "RHI/IBuffer.h"
#include <memory>

class IInputLayout;

class GrassRenderPass : public IRenderPass {
public:
    GrassRenderPass() = default;
    ~GrassRenderPass() override;

    std::string GetName() const override { return "GrassRenderPass"; }
    bool HasSideEffects() const override { return true; }

    void Initialize();

    void Setup(FrameGraphBuilder& builder, const RenderContext& rc) override;
    void Execute(FrameGraphResources& resources, const RenderQueue& queue, RenderContext& rc) override;

private:
    ResourceHandle m_hGBuffer0;
    ResourceHandle m_hGBuffer1;
    ResourceHandle m_hGBuffer2;
    ResourceHandle m_hGBuffer3;
    ResourceHandle m_hDepth;

    std::unique_ptr<IShader>        m_vs;
    std::unique_ptr<IShader>        m_ps;
    std::unique_ptr<IInputLayout>   m_inputLayout;
    std::unique_ptr<IPipelineState> m_pso;
    std::unique_ptr<IBuffer>        m_cb;

    bool m_initialized = false;
};
