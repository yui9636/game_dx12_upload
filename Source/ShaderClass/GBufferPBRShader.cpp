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
#include "RHI/DX12/DX12Device.h"
#include "RHI/DX12/DX12RootSignature.h"

namespace {
    D3D12_CPU_DESCRIPTOR_HANDLE OffsetHandle(D3D12_CPU_DESCRIPTOR_HANDLE base, UINT stride, UINT index)
    {
        base.ptr += static_cast<SIZE_T>(stride) * index;
        return base;
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

    if (Graphics::Instance().GetAPI() == GraphicsAPI::DX12) {
        auto* dx12Device = Graphics::Instance().GetDX12Device();
        auto* d3dDevice = dx12Device ? dx12Device->GetDevice() : nullptr;
        if (d3dDevice) {
            m_dx12SrvDescriptorSize = dx12Device->GetCBVSRVUAVDescriptorSize();

            D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
            heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
            heapDesc.NumDescriptors = 4;
            heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
            HRESULT hr = d3dDevice->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&m_dx12SrvHeap));
            if (SUCCEEDED(hr) && m_dx12SrvHeap) {
                auto stagingNull = dx12Device->AllocateSRVDescriptor();
                D3D12_SHADER_RESOURCE_VIEW_DESC null2DDesc = {};
                null2DDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
                null2DDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
                null2DDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
                null2DDesc.Texture2D.MipLevels = 1;
                d3dDevice->CreateShaderResourceView(nullptr, &null2DDesc, stagingNull);
                m_dx12NullSrv2D = stagingNull;

                auto cpuBase = m_dx12SrvHeap->GetCPUDescriptorHandleForHeapStart();
                m_dx12SrvGpuBase = m_dx12SrvHeap->GetGPUDescriptorHandleForHeapStart();
                for (UINT slot = 0; slot < 4; ++slot) {
                    auto dst = OffsetHandle(cpuBase, m_dx12SrvDescriptorSize, slot);
                    d3dDevice->CopyDescriptorsSimple(1, dst, m_dx12NullSrv2D, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
                }
            }
        }
    }
}


void GBufferPBRShader::Begin(const RenderContext& rc)
{
    rc.commandList->SetPipelineState(m_pso.get());
    rc.commandList->PSSetConstantBuffer(1, m_meshConstantBuffer.get());
}


void GBufferPBRShader::Update(const RenderContext& rc, const ModelResource::MeshResource& mesh)

{

    CbMesh cbMesh{};

    const auto meshColor = mesh.material.color;

    cbMesh.materialColor = {

        meshColor.x * m_matColor.x,

        meshColor.y * m_matColor.y,

        meshColor.z * m_matColor.z,

        meshColor.w * m_matColor.w

    };

    cbMesh.metallicFactor = mesh.material.metallicFactor * m_matMetallic;

    cbMesh.roughnessFactor = mesh.material.roughnessFactor * m_matRoughness;

    cbMesh.emissiveFactor = m_matEmissive;



    rc.commandList->UpdateBuffer(m_meshConstantBuffer.get(), &cbMesh, sizeof(cbMesh));



    ITexture* srvs[] = {

        mesh.material.albedoMap.get(),

        mesh.material.normalMap.get(),

        mesh.material.metallicMap.get(),

        mesh.material.roughnessMap.get()

    };



    static int s_loggedMaterialCount = 0;

    if (s_loggedMaterialCount < 6) {

        LOG_INFO("[GBufferPBRShader] material='%s' albedoFile='%s' normalFile='%s' color=(%.3f, %.3f, %.3f, %.3f) metallic=%.3f roughness=%.3f albedo=%p normal=%p metallicTex=%p roughnessTex=%p",

            mesh.material.name.c_str(),

            mesh.material.albedoTextureFileName.c_str(),

            mesh.material.normalTextureFileName.c_str(),

            cbMesh.materialColor.x, cbMesh.materialColor.y, cbMesh.materialColor.z, cbMesh.materialColor.w,

            cbMesh.metallicFactor, cbMesh.roughnessFactor,

            srvs[0], srvs[1], srvs[2], srvs[3]);

        ++s_loggedMaterialCount;

    }



    if (Graphics::Instance().GetAPI() == GraphicsAPI::DX12) {
        auto* dx12Cmd = static_cast<DX12CommandList*>(rc.commandList);
        if (dx12Cmd && m_dx12SrvHeap) {
            auto* d3dDevice = Graphics::Instance().GetDX12Device()->GetDevice();
            auto cpuBase = m_dx12SrvHeap->GetCPUDescriptorHandleForHeapStart();
            for (UINT slot = 0; slot < 4; ++slot) {
                auto dst = OffsetHandle(cpuBase, m_dx12SrvDescriptorSize, slot);
                D3D12_CPU_DESCRIPTOR_HANDLE src = m_dx12NullSrv2D;
                if (srvs[slot]) {
                    auto* dx12Tex = static_cast<DX12Texture*>(srvs[slot]);
                    if (dx12Tex->HasSRV()) {
                        src = dx12Tex->GetSRV();
                    }
                }
                d3dDevice->CopyDescriptorsSimple(1, dst, src, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
            }

            ID3D12DescriptorHeap* heaps[] = { m_dx12SrvHeap.Get() };
            dx12Cmd->GetNativeCommandList()->SetDescriptorHeaps(1, heaps);
            dx12Cmd->GetNativeCommandList()->SetGraphicsRootDescriptorTable(DX12RootSignature::SRVTable, m_dx12SrvGpuBase);
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
    }

    ITexture* nullTextures[] = { nullptr, nullptr, nullptr, nullptr };
    rc.commandList->PSSetTextures(0, _countof(nullTextures), nullTextures);
}
