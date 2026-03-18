#include "GBufferPBRShader.h"
#include "Graphics.h"
#include "Console/Logger.h"
#include "RHI/IResourceFactory.h"
#include "RHI/ICommandList.h"
#include "RHI/ITexture.h"
#include "RHI/IBuffer.h"
#include "RHI/PipelineStateDesc.h"
#include "RHI/IPipelineState.h"
#include "RHI/DX12/DX12CommandList.h"
#include "RHI/DX12/DX12Texture.h"
#include "RHI/DX12/DX12RootSignature.h"

namespace {
void CopyOrCreateNull2D(ID3D12Device* device,
                        D3D12_CPU_DESCRIPTOR_HANDLE dst,
                        ITexture* texture)
{
    if (texture) {
        auto* dx12Tex = dynamic_cast<DX12Texture*>(texture);
        if (dx12Tex && dx12Tex->HasSRV()) {
            device->CopyDescriptorsSimple(1, dst, dx12Tex->GetSRV(), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
            return;
        }
    }

    D3D12_SHADER_RESOURCE_VIEW_DESC nullDesc = {};
    nullDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    nullDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    nullDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    nullDesc.Texture2D.MipLevels = 1;
    device->CreateShaderResourceView(nullptr, &nullDesc, dst);
}
}

GBufferPBRShader::GBufferPBRShader(IResourceFactory* factory)
    : PBRShader(factory)
{
    m_vs = factory->CreateShader(ShaderType::Vertex, "Data/Shader/PBRVS.cso");
    m_ps = factory->CreateShader(ShaderType::Pixel, "Data/Shader/GBufferPBRPS.cso");

    InputLayoutElement layoutElements[] = {
        {"POSITION", 0, TextureFormat::R32G32B32_FLOAT, 0, kAppendAlignedElement},
        {"BONE_WEIGHTS", 0, TextureFormat::R32G32B32A32_FLOAT, 0, kAppendAlignedElement},
        {"BONE_INDICES", 0, TextureFormat::R32G32B32A32_UINT, 0, kAppendAlignedElement},
        {"TEXCOORD", 0, TextureFormat::R32G32_FLOAT, 0, kAppendAlignedElement},
        {"NORMAL", 0, TextureFormat::R32G32B32_FLOAT, 0, kAppendAlignedElement},
        {"TANGENT", 0, TextureFormat::R32G32B32_FLOAT, 0, kAppendAlignedElement},
    };
    InputLayoutDesc layoutDesc = { layoutElements, _countof(layoutElements) };
    m_inputLayout = factory->CreateInputLayout(layoutDesc, m_vs.get());

    m_meshConstantBuffer = factory->CreateBuffer(sizeof(CbMesh), BufferType::Constant);

    PipelineStateDesc desc{};
    desc.vertexShader = m_vs.get();
    desc.pixelShader = m_ps.get();
    desc.inputLayout = m_inputLayout.get();
    desc.primitiveTopology = PrimitiveTopology::TriangleList;
    desc.numRenderTargets = 4;
    desc.rtvFormats[0] = TextureFormat::R16G16B16A16_FLOAT;
    desc.rtvFormats[1] = TextureFormat::R16G16B16A16_FLOAT;
    desc.rtvFormats[2] = TextureFormat::R32G32B32A32_FLOAT;
    desc.rtvFormats[3] = TextureFormat::R32G32_FLOAT;
    desc.dsvFormat = TextureFormat::D32_FLOAT;

    auto* rs = Graphics::Instance().GetRenderState();
    desc.rasterizerState = rs->GetRasterizerState(RasterizerState::SolidCullBack);
    desc.depthStencilState = rs->GetDepthStencilState(DepthState::TestAndWrite);
    desc.blendState = rs->GetBlendState(BlendState::Opaque);

    m_pso = factory->CreatePipelineState(desc);
}

void GBufferPBRShader::EnsureDx12MaterialHeap()
{
    if (m_dx12MaterialHeap || Graphics::Instance().GetAPI() != GraphicsAPI::DX12) {
        return;
    }

    auto* dx12Device = Graphics::Instance().GetDX12Device();
    if (!dx12Device) {
        return;
    }

    D3D12_DESCRIPTOR_HEAP_DESC desc = {};
    desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    desc.NumDescriptors = 4;
    desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    HRESULT hr = dx12Device->GetDevice()->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&m_dx12MaterialHeap));
    if (FAILED(hr)) {
        LOG_ERROR("[GBufferPBRShader] Failed to create DX12 material heap hr=0x%08X", hr);
        return;
    }

    m_dx12SrvDescriptorSize = dx12Device->GetCBVSRVUAVDescriptorSize();
}

void GBufferPBRShader::Begin(const RenderContext& rc)
{
    rc.commandList->SetPipelineState(m_pso.get());
    rc.commandList->PSSetConstantBuffer(1, m_meshConstantBuffer.get());
}

void GBufferPBRShader::Update(const RenderContext& rc, const Model::Mesh& mesh)
{
    CbMesh cbMesh{};
    const auto meshColor = mesh.material ? mesh.material->color : DirectX::XMFLOAT4{ 1.0f, 1.0f, 1.0f, 1.0f };
    cbMesh.materialColor = {
        meshColor.x * m_matColor.x,
        meshColor.y * m_matColor.y,
        meshColor.z * m_matColor.z,
        meshColor.w * m_matColor.w
    };
    cbMesh.metallicFactor = (mesh.material ? mesh.material->metallicFactor : 1.0f) * m_matMetallic;
    cbMesh.roughnessFactor = (mesh.material ? mesh.material->roughnessFactor : 1.0f) * m_matRoughness;
    cbMesh.emissiveFactor = m_matEmissive;

    rc.commandList->UpdateBuffer(m_meshConstantBuffer.get(), &cbMesh, sizeof(cbMesh));

    ITexture* srvs[] = {
        mesh.material->albedoMap.get(),
        mesh.material->normalMap.get(),
        mesh.material->metallicMap.get(),
        mesh.material->roughnessMap.get()
    };

    static int s_loggedMaterialCount = 0;
    if (s_loggedMaterialCount < 6) {
        LOG_INFO("[GBufferPBRShader] material='%s' albedoFile='%s' normalFile='%s' color=(%.3f, %.3f, %.3f, %.3f) metallic=%.3f roughness=%.3f albedo=%p normal=%p metallicTex=%p roughnessTex=%p",
            mesh.material ? mesh.material->name.c_str() : "<null>",
            mesh.material ? mesh.material->albedoTextureFileName.c_str() : "",
            mesh.material ? mesh.material->normalTextureFileName.c_str() : "",
            cbMesh.materialColor.x, cbMesh.materialColor.y, cbMesh.materialColor.z, cbMesh.materialColor.w,
            cbMesh.metallicFactor, cbMesh.roughnessFactor,
            srvs[0], srvs[1], srvs[2], srvs[3]);
        ++s_loggedMaterialCount;
    }

    if (Graphics::Instance().GetAPI() == GraphicsAPI::DX12) {
        EnsureDx12MaterialHeap();
        auto* dx12Cmd = static_cast<DX12CommandList*>(rc.commandList);
        auto* dx12Device = Graphics::Instance().GetDX12Device();
        if (m_dx12MaterialHeap && dx12Cmd && dx12Device) {
            ID3D12DescriptorHeap* heaps[] = { m_dx12MaterialHeap.Get() };
            dx12Cmd->GetNativeCommandList()->SetDescriptorHeaps(1, heaps);

            D3D12_CPU_DESCRIPTOR_HANDLE cpu = m_dx12MaterialHeap->GetCPUDescriptorHandleForHeapStart();
            D3D12_GPU_DESCRIPTOR_HANDLE gpu = m_dx12MaterialHeap->GetGPUDescriptorHandleForHeapStart();
            for (UINT i = 0; i < 4; ++i) {
                D3D12_CPU_DESCRIPTOR_HANDLE dst = cpu;
                dst.ptr += static_cast<SIZE_T>(i) * m_dx12SrvDescriptorSize;
                CopyOrCreateNull2D(dx12Device->GetDevice(), dst, srvs[i]);
            }
            dx12Cmd->GetNativeCommandList()->SetGraphicsRootDescriptorTable(DX12RootSignature::SRVTable, gpu);
            return;
        }
    }

    rc.commandList->PSSetTextures(0, _countof(srvs), srvs);
}

void GBufferPBRShader::End(const RenderContext& rc)
{
    if (Graphics::Instance().GetAPI() == GraphicsAPI::DX12) {
        auto* dx12Cmd = static_cast<DX12CommandList*>(rc.commandList);
        if (dx12Cmd) {
            dx12Cmd->RestoreFrameDescriptorHeap();
        }
        return;
    }

    ITexture* nullTextures[] = { nullptr, nullptr, nullptr, nullptr };
    rc.commandList->PSSetTextures(0, _countof(nullTextures), nullTextures);
}
