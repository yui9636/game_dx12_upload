#pragma once

#include <memory>
#include "IRenderPass.h"
#include "RenderGraph/FrameGraphTypes.h"

class EffectMeshShader;

class EffectMeshPass : public IRenderPass
{
public:
    EffectMeshPass();
    ~EffectMeshPass() override;

    std::string GetName() const override { return "EffectMeshPass"; }
    void Setup(FrameGraphBuilder& builder, const RenderContext& rc) override;
    void Execute(FrameGraphResources& resources, const RenderQueue& queue, RenderContext& rc) override;

private:
    ResourceHandle m_hSceneColor;
    ResourceHandle m_hDepth;
    std::unique_ptr<EffectMeshShader> m_shader;
};
