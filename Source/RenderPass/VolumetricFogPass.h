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

    void Setup(FrameGraphBuilder& builder) override;
    void Execute(FrameGraphResources& resources, const RenderQueue& queue, RenderContext& rc) override;

private:
    std::unique_ptr<IShader> m_vs;
    std::unique_ptr<IShader> m_psRaymarch;
    std::unique_ptr<IShader> m_psBlur;

    std::unique_ptr<IPipelineState> m_psoRaymarch;
    std::unique_ptr<IPipelineState> m_psoBlur;

    // ====================================================
    // �� �O���t�ł���肷��`�P�b�g
    // ====================================================
    ResourceHandle m_hGBuffer2;          // ���́FWorldPos & Depth
    ResourceHandle m_hVolumetricFog;     // ���ԁF���t�H�O�i�n�[�t�𑜓x�j
    ResourceHandle m_hVolumetricFogBlur; // �o�́F�u���[��t�H�O�i�n�[�t�𑜓x�j
};