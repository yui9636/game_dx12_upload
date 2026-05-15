#pragma once
#include "RenderPass/IRenderPass.h"
#include "RenderGraph/FrameGraphTypes.h"
#include "RHI/IPipelineState.h"
#include "RHI/IState.h"
#include "RHI/IShader.h"
#include "RHI/IBuffer.h"
#include <memory>

class IInputLayout;

class WaterRenderPass : public IRenderPass {
public:
    WaterRenderPass() = default;
    ~WaterRenderPass() override;

    std::string GetName() const override { return "WaterRenderPass"; }
    bool HasSideEffects() const override { return true; }

    void Initialize();

    void Setup(FrameGraphBuilder& builder, const RenderContext& rc) override;
    void Execute(FrameGraphResources& resources, const RenderQueue& queue, RenderContext& rc) override;

private:
    ResourceHandle m_hSceneColor;
    ResourceHandle m_hDepth;
    ResourceHandle m_hGBuffer2;     // WorldPos / linear depth (used for true water depth)
    ResourceHandle m_hPrevScene;    // Previous frame scene (used for screen-space refraction)

    std::unique_ptr<IShader>        m_vs;
    std::unique_ptr<IShader>        m_ps;
    std::unique_ptr<IInputLayout>   m_inputLayout;
    std::unique_ptr<IPipelineState> m_pso;
    std::unique_ptr<IBuffer>        m_cbWater;

    bool m_initialized = false;
};
