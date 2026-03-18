#pragma once
#include "RenderPass/IRenderPass.h"
#include "RenderGraph/FrameGraphTypes.h"
#include <memory>

class IShader;
class IPipelineState;
class IResourceFactory;

class SSGIPass : public IRenderPass {
public:
    SSGIPass(IResourceFactory* factory);
    ~SSGIPass() override;

    std::string GetName() const override { return "SSGIPass"; }

    void Setup(FrameGraphBuilder& builder) override;
    void Execute(FrameGraphResources& resources, const RenderQueue& queue, RenderContext& rc) override;

private:
    std::unique_ptr<IShader> m_vs;
    std::unique_ptr<IShader> m_psRaymarch;
    std::unique_ptr<IShader> m_psBlur;

    std::unique_ptr<IPipelineState> m_psoRaymarch;
    std::unique_ptr<IPipelineState> m_psoBlur;

    // ====================================================
    // �� �O���t�ŊǗ�����`�P�b�g
    // ====================================================
    ResourceHandle m_hGBuffer1;  // Normal
    ResourceHandle m_hGBuffer2;  // WorldPos
    ResourceHandle m_hPrevScene; // ���ˁE�o�E���X��

    ResourceHandle m_hSSGI;      // ���ԁF��SSGI�i�n�[�t�𑜓x�j
    ResourceHandle m_hSSGIBlur;  // �o�́F�u���[��SSGI�i�n�[�t�𑜓x�j
};