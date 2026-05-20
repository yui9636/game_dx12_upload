// FinalBlitPass のレンダーパス宣言をまとめます。
#pragma once
#include "IRenderPass.h"
#include "RenderGraph/FrameGraphTypes.h"
#include <memory>

class IShader;
class IPipelineState;
class IResourceFactory;

// HDR SceneColor を DisplayColor へコピーする最終 blit pass。
class FinalBlitPass : public IRenderPass
{
public:
    explicit FinalBlitPass(IResourceFactory* factory);
    ~FinalBlitPass() override = default;

    std::string GetName() const override { return "FinalBlitPass"; }
    bool HasSideEffects() const override { return true; }

    void Setup(FrameGraphBuilder& builder, const RenderContext& rc) override;
    void Execute(FrameGraphResources& resources, const RenderQueue& queue, RenderContext& rc) override;

    IPipelineState* GetPSO() const { return m_pso.get(); }

private:
    // full-screen quad 用 shader / PSO。
    std::shared_ptr<IShader> m_vs;
    std::shared_ptr<IShader> m_ps;
    std::shared_ptr<IPipelineState> m_pso;
    ResourceHandle m_hSceneColor;   // 入力 HDR scene color。
    ResourceHandle m_hDisplayColor; // 出力 backbuffer 相当の display color。
};
