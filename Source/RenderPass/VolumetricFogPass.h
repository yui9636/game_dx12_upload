#pragma once
#include "RenderPass/IRenderPass.h"
#include "RenderGraph/FrameGraphTypes.h"
#include <memory>

class IShader;
class IPipelineState;
class IResourceFactory;

class VolumetricFogPass : public IRenderPass {
public:
    VolumetricFogPass(IResourceFactory* factory);
    ~VolumetricFogPass() override;

    std::string GetName() const override { return "VolumetricFogPass"; }

    void Setup(FrameGraphBuilder& builder, const RenderContext& rc) override;
    void Execute(FrameGraphResources& resources, const RenderQueue& queue, RenderContext& rc) override;

private:
    std::unique_ptr<IShader> m_vs;
    std::unique_ptr<IShader> m_psRaymarch;
    std::unique_ptr<IShader> m_psBlur;

    std::unique_ptr<IPipelineState> m_psoRaymarch;
    std::unique_ptr<IPipelineState> m_psoBlur;

    // ====================================================
    // ====================================================
    ResourceHandle m_hGBuffer2;
    ResourceHandle m_hVolumetricFog;
    ResourceHandle m_hVolumetricFogBlur;
};
