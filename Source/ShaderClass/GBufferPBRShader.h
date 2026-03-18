#pragma once

#include "ShaderClass/PBRShader.h"
#include <memory>
#include <d3d12.h>
#include <wrl/client.h>

class IResourceFactory;
class IShader;
class IInputLayout;
class IBuffer;
class IPipelineState;

class GBufferPBRShader : public PBRShader
{
public:
    GBufferPBRShader(IResourceFactory* factory);
    ~GBufferPBRShader() override = default;

    void Begin(const RenderContext& rc) override;
    void Update(const RenderContext& rc, const ModelResource::MeshResource& mesh) override;
    void End(const RenderContext& rc) override;

private:
    struct CbMesh {
        DirectX::XMFLOAT4 materialColor;
        float metallicFactor;
        float roughnessFactor;
        float emissiveFactor;
        float padding;
    };

    std::unique_ptr<IShader> m_vs;
    std::unique_ptr<IShader> m_ps;
    std::unique_ptr<IInputLayout> m_inputLayout;
    std::unique_ptr<IBuffer> m_meshConstantBuffer;
    std::unique_ptr<IPipelineState> m_pso;

    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_dx12SrvHeap;
    UINT m_dx12SrvDescriptorSize = 0;
    D3D12_GPU_DESCRIPTOR_HANDLE m_dx12SrvGpuBase = {};
    D3D12_CPU_DESCRIPTOR_HANDLE m_dx12NullSrv2D = {};
};
