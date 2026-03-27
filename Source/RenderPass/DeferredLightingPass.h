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

class DeferredLightingPass : public IRenderPass {
public:
    DeferredLightingPass(IResourceFactory* factory);
    ~DeferredLightingPass() override;

    std::string GetName() const override { return "DeferredLightingPass"; }

    void Setup(FrameGraphBuilder& builder, const RenderContext& rc) override;
    void Execute(FrameGraphResources& resources, const RenderQueue& queue, RenderContext& rc) override;

private:
    std::unique_ptr<IShader> m_vs;
    std::unique_ptr<IShader> m_ps;
    std::unique_ptr<IPipelineState> m_pso;
    std::shared_ptr<ITexture> m_lutGGX;
    std::shared_ptr<ITexture> m_whiteFallback;

    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_dx12SrvHeap;
    UINT m_dx12SrvDescriptorSize = 0;
    D3D12_GPU_DESCRIPTOR_HANDLE m_dx12SrvGpuBase = {};
    D3D12_CPU_DESCRIPTOR_HANDLE m_dx12NullSrv2D = {};
    D3D12_CPU_DESCRIPTOR_HANDLE m_dx12NullSrv2DArray = {};
    D3D12_CPU_DESCRIPTOR_HANDLE m_dx12NullSrvCube = {};

    // ====================================================
    // ====================================================
    ResourceHandle m_hGBuffer0; // Albedo
    ResourceHandle m_hGBuffer1; // Normal
    ResourceHandle m_hGBuffer2; // WorldPos
    ResourceHandle m_hGBuffer3; // Velocity (GBufferVelocity)
    ResourceHandle m_hDepth;    // Depth

    ResourceHandle m_hGTAO;
    ResourceHandle m_hSSGI;
    ResourceHandle m_hFog;
    ResourceHandle m_hSSR;

    ResourceHandle m_hSceneColor;
};
