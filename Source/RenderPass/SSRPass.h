#pragma once
#include "RenderPass/IRenderPass.h"
#include "RenderGraph/FrameGraphTypes.h"
#include <memory>

class IShader;
class IPipelineState;
class IResourceFactory;

// Screen Space Reflection を半解像度で生成し、blur 結果を lighting へ渡す pass。
class SSRPass : public IRenderPass {
public:
    SSRPass(IResourceFactory* factory);
    ~SSRPass() override;

    std::string GetName() const override { return "SSRPass"; }

    void Setup(FrameGraphBuilder& builder, const RenderContext& rc) override;
    void Execute(FrameGraphResources& resources, const RenderQueue& queue, RenderContext& rc) override;

private:
    // raymarch と blur の 2 段構成。
    std::unique_ptr<IShader> m_vs;
    std::unique_ptr<IShader> m_psRaymarch;
    std::unique_ptr<IShader> m_psBlur;

    std::unique_ptr<IPipelineState> m_psoRaymarch;
    std::unique_ptr<IPipelineState> m_psoBlur;
    // FrameGraph で管理する入力/出力 handle。
    ResourceHandle m_hGBuffer0;    // Albedo 用。
    ResourceHandle m_hGBuffer1;    // 法線。
    ResourceHandle m_hGBuffer2;    // WorldPos 用。
    ResourceHandle m_hPrevScene;   // 反射元の前フレーム色。

    ResourceHandle m_hSSR;         // pass 1 の生 SSR。
    ResourceHandle m_hSSRBlur;     // pass 2 の blur 済み結果。
};
