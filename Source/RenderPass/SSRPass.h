#pragma once
#include "RenderPass/IRenderPass.h"
#include "RenderGraph/FrameGraphTypes.h"
#include <memory>

class IShader;
class IPipelineState;
class IResourceFactory;

class SSRPass : public IRenderPass {
public:
    SSRPass(IResourceFactory* factory);
    ~SSRPass() override;

    std::string GetName() const override { return "SSRPass"; }

    void Setup(FrameGraphBuilder& builder) override;
    void Execute(FrameGraphResources& resources, const RenderQueue& queue, RenderContext& rc) override;

private:
    std::unique_ptr<IShader> m_vs;
    std::unique_ptr<IShader> m_psRaymarch;
    std::unique_ptr<IShader> m_psBlur;

    std::unique_ptr<IPipelineState> m_psoRaymarch;
    std::unique_ptr<IPipelineState> m_psoBlur;

    // ====================================================
    // ★ グラフで管理するハンドル達
    // ====================================================
    ResourceHandle m_hGBuffer0;    // Albedo
    ResourceHandle m_hGBuffer1;    // Normal
    ResourceHandle m_hGBuffer2;    // WorldPos
    ResourceHandle m_hPrevScene;   // 反射元の色

    ResourceHandle m_hSSR;         // パス1の出力（生SSR）
    ResourceHandle m_hSSRBlur;     // パス2の出力（最終結果）
};