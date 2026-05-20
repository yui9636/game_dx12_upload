#pragma once
#include "RenderPass/IRenderPass.h"
#include "RenderGraph/FrameGraphTypes.h"
#include <memory>

class IShader;
class IPipelineState;
class IResourceFactory;

// Screen Space Global Illumination を半解像度で生成し、blur 結果を lighting へ渡す pass。
class SSGIPass : public IRenderPass {
public:
    SSGIPass(IResourceFactory* factory);
    ~SSGIPass() override;

    std::string GetName() const override { return "SSGIPass"; }

    void Setup(FrameGraphBuilder& builder, const RenderContext& rc) override;
    void Execute(FrameGraphResources& resources, const RenderQueue& queue, RenderContext& rc) override;

private:
    // raymarch と blur の 2 段構成。
    std::unique_ptr<IShader> m_vs;
    std::unique_ptr<IShader> m_psRaymarch;
    std::unique_ptr<IShader> m_psBlur;

    std::unique_ptr<IPipelineState> m_psoRaymarch;
    std::unique_ptr<IPipelineState> m_psoBlur;
    ResourceHandle m_hGBuffer1;  // 法線。
    ResourceHandle m_hGBuffer2;  // WorldPos 用。
    ResourceHandle m_hPrevScene; // history / fallback color。

    ResourceHandle m_hSSGI;     // 生 SSGI。
    ResourceHandle m_hSSGIBlur; // lighting 入力用の blur 済み SSGI。
};
