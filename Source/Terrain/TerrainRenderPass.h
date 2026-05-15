#pragma once
#include "RenderPass/IRenderPass.h"
#include "RenderGraph/FrameGraphTypes.h"
#include "RHI/IPipelineState.h"
#include "RHI/IState.h"
#include "RHI/IShader.h"
#include "RHI/IBuffer.h"
#include "RHI/ITexture.h"
#include <memory>

class IInputLayout;

class TerrainRenderPass : public IRenderPass {
public:
    TerrainRenderPass() = default;
    ~TerrainRenderPass() override;

    std::string GetName() const override { return "TerrainRenderPass"; }
    bool HasSideEffects() const override { return true; }

    void Initialize();

    void Setup(FrameGraphBuilder& builder, const RenderContext& rc) override;
    void Execute(FrameGraphResources& resources, const RenderQueue& queue, RenderContext& rc) override;

private:
    // GBuffer write targets (terrain writes to the deferred GBuffer like opaque meshes do).
    ResourceHandle m_hGBuffer0;
    ResourceHandle m_hGBuffer1;
    ResourceHandle m_hGBuffer2;
    ResourceHandle m_hGBuffer3;
    ResourceHandle m_hDepth;

    std::unique_ptr<IShader>            m_vs;
    std::unique_ptr<IShader>            m_ps;
    std::unique_ptr<IInputLayout>       m_inputLayout;
    std::unique_ptr<IPipelineState>     m_pso;
    std::unique_ptr<IBuffer>            m_cbTerrain;
    std::unique_ptr<IBuffer>            m_cbMaterial;
    std::unique_ptr<ITexture>           m_defaultWhite;       // 1x1 white -> albedo fallback
    std::unique_ptr<ITexture>           m_defaultFlatNormal;  // (128,128,255) flat tangent normal
    std::unique_ptr<ITexture>           m_defaultMRA;         // (0,217,255) M=0 R=0.85 AO=1.0

    bool m_initialized = false;
};
