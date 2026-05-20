#pragma once
#include "RenderPass/IRenderPass.h"
#include "RenderGraph/FrameGraphTypes.h"
#include <memory>

class IShader;
class IPipelineState;
class IBuffer;
class IResourceFactory;

// Projects DecalComponent textures onto the G-buffer. Runs after all opaque geometry
// has written the G-buffer (GBuffer/Terrain/Grass) and before lighting consumes it, so
// decals appear as if painted onto surface albedo and are lit normally afterwards.
class DeferredDecalPass : public IRenderPass {
public:
    DeferredDecalPass(IResourceFactory* factory);
    ~DeferredDecalPass() override;

    std::string GetName() const override { return "DeferredDecalPass"; }

    void Setup(FrameGraphBuilder& builder, const RenderContext& rc) override;
    void Execute(FrameGraphResources& resources, const RenderQueue& queue, RenderContext& rc) override;

private:
    std::unique_ptr<IShader> m_vs;
    std::unique_ptr<IShader> m_ps;
    std::unique_ptr<IPipelineState> m_pso;
    std::unique_ptr<IBuffer> m_cb;

    ResourceHandle m_hGBuffer0; // Albedo (blended into).
    ResourceHandle m_hGBuffer1; // Normal (read for angle fade).
    ResourceHandle m_hGBuffer2; // World position (read for projection).
};
