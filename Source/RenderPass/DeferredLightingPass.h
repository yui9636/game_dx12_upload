#pragma once
#include "RenderPass/IRenderPass.h"
#include "RenderGraph/FrameGraphTypes.h"
#include <d3d12.h>
#include <wrl/client.h>
#include <memory>

class IShader;
class IPipelineState;
class ITexture;
class IResourceFactory;

// GBuffer、AO、SSGI、Fog、SSR、IBL、Shadow を合成して HDR SceneColor を作る pass。
class DeferredLightingPass : public IRenderPass {
public:
    DeferredLightingPass(IResourceFactory* factory);
    ~DeferredLightingPass() override;

    std::string GetName() const override { return "DeferredLightingPass"; }

    void Setup(FrameGraphBuilder& builder, const RenderContext& rc) override;
    void Execute(FrameGraphResources& resources, const RenderQueue& queue, RenderContext& rc) override;

private:
    // full-screen triangle 用 shader / PSO。
    std::unique_ptr<IShader> m_vs;
    std::unique_ptr<IShader> m_ps;
    std::unique_ptr<IPipelineState> m_pso;
    std::shared_ptr<ITexture> m_lutGGX;        // BRDF LUT。
    std::shared_ptr<ITexture> m_whiteFallback; // optional AO が無い場合の白 texture。

    // DX12 の null SRV を含む固定 descriptor heap。欠けた optional texture を安全に bind する。
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_dx12SrvHeap;
    UINT m_dx12SrvDescriptorSize = 0;
    D3D12_GPU_DESCRIPTOR_HANDLE m_dx12SrvGpuBase = {};
    D3D12_CPU_DESCRIPTOR_HANDLE m_dx12NullSrv2D = {};
    D3D12_CPU_DESCRIPTOR_HANDLE m_dx12NullSrv2DArray = {};
    D3D12_CPU_DESCRIPTOR_HANDLE m_dx12NullSrvCube = {};
    ResourceHandle m_hGBuffer0; // Albedo 用。
    ResourceHandle m_hGBuffer1; // 法線。
    ResourceHandle m_hGBuffer2; // WorldPos 用。
    ResourceHandle m_hGBuffer3; // Velocity。GBufferVelocity に対応する。
    ResourceHandle m_hDepth;    // Depth 用。

    ResourceHandle m_hGTAO; // optional AO。
    ResourceHandle m_hSSGI; // optional screen-space GI。
    ResourceHandle m_hFog;  // optional volumetric fog。
    ResourceHandle m_hSSR;  // optional screen-space reflection。

    ResourceHandle m_hSceneColor; // lighting 出力先の HDR scene color。
};
