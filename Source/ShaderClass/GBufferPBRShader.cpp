#include "GBufferPBRShader.h"

#include "Graphics.h"
#include "Console/Logger.h"
#include "Material/MaterialAsset.h"
#include "System/ResourceManager.h"
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
    m_instancedVs = factory->CreateShader(ShaderType::Vertex, "Data/Shader/PBRInstancedVS.cso");

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

    InputLayoutElement instancedLayoutElements[] = {
        {"POSITION", 0, TextureFormat::R32G32B32_FLOAT, 0, kAppendAlignedElement},
        {"BONE_WEIGHTS", 0, TextureFormat::R32G32B32A32_FLOAT, 0, kAppendAlignedElement},
        {"BONE_INDICES", 0, TextureFormat::R32G32B32A32_UINT, 0, kAppendAlignedElement},
        {"TEXCOORD", 0, TextureFormat::R32G32_FLOAT, 0, kAppendAlignedElement},
        {"NORMAL", 0, TextureFormat::R32G32B32_FLOAT, 0, kAppendAlignedElement},
        {"TANGENT", 0, TextureFormat::R32G32B32_FLOAT, 0, kAppendAlignedElement},
        {"INSTANCE_WORLD", 0, TextureFormat::R32G32B32A32_FLOAT, 1, kAppendAlignedElement, true, 1},
        {"INSTANCE_WORLD", 1, TextureFormat::R32G32B32A32_FLOAT, 1, kAppendAlignedElement, true, 1},
        {"INSTANCE_WORLD", 2, TextureFormat::R32G32B32A32_FLOAT, 1, kAppendAlignedElement, true, 1},
        {"INSTANCE_WORLD", 3, TextureFormat::R32G32B32A32_FLOAT, 1, kAppendAlignedElement, true, 1},
        {"INSTANCE_PREV_WORLD", 0, TextureFormat::R32G32B32A32_FLOAT, 1, kAppendAlignedElement, true, 1},
        {"INSTANCE_PREV_WORLD", 1, TextureFormat::R32G32B32A32_FLOAT, 1, kAppendAlignedElement, true, 1},
        {"INSTANCE_PREV_WORLD", 2, TextureFormat::R32G32B32A32_FLOAT, 1, kAppendAlignedElement, true, 1},
        {"INSTANCE_PREV_WORLD", 3, TextureFormat::R32G32B32A32_FLOAT, 1, kAppendAlignedElement, true, 1},
    };
    InputLayoutDesc instancedLayoutDesc = { instancedLayoutElements, _countof(instancedLayoutElements) };
    m_instancedInputLayout = factory->CreateInputLayout(instancedLayoutDesc, m_instancedVs.get());

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

    PipelineStateDesc instancedDesc = desc;
    instancedDesc.vertexShader = m_instancedVs.get();
    instancedDesc.inputLayout = m_instancedInputLayout.get();
    m_instancedPso = factory->CreatePipelineState(instancedDesc);

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
    if (Graphics::Instance().GetAPI() != GraphicsAPI::DX12) {
        rc.commandList->PSSetConstantBuffer(1, m_meshConstantBuffer.get());
    }
}

void GBufferPBRShader::BeginInstanced(const RenderContext& rc)
{
    rc.commandList->SetPipelineState(m_instancedPso ? m_instancedPso.get() : m_pso.get());
    if (Graphics::Instance().GetAPI() != GraphicsAPI::DX12) {
        rc.commandList->PSSetConstantBuffer(1, m_meshConstantBuffer.get());
    }
}

bool GBufferPBRShader::SupportsInstancing(const ModelResource::MeshResource& mesh) const
{
    return mesh.bones.empty();
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

    auto* dx12Cmd = Graphics::Instance().GetAPI() == GraphicsAPI::DX12
        ? static_cast<DX12CommandList*>(rc.commandList)
        : nullptr;
    if (dx12Cmd) {
        dx12Cmd->PSSetDynamicConstantBuffer(1, &cbMesh, sizeof(cbMesh));
    } else {
        rc.commandList->UpdateBuffer(m_meshConstantBuffer.get(), &cbMesh, sizeof(cbMesh));
    }

    auto resolveTexture = [&](const std::string& path, std::shared_ptr<ITexture> fallback) -> ITexture* {
        if (m_materialOverride && !path.empty()) {
            if (auto texture = ResourceManager::Instance().GetTexture(path)) {
                return texture.get();
            }
        }
        return fallback.get();
    };

    ITexture* srvs[] = {
        resolveTexture(m_materialOverride ? m_materialOverride->diffuseTexturePath : std::string(), mesh.material.albedoMap),
        resolveTexture(m_materialOverride ? m_materialOverride->normalTexturePath : std::string(), mesh.material.normalMap),
        resolveTexture(m_materialOverride ? m_materialOverride->metallicRoughnessTexturePath : std::string(), mesh.material.metallicMap),
        resolveTexture(m_materialOverride ? m_materialOverride->metallicRoughnessTexturePath : std::string(), mesh.material.roughnessMap)
    };

    // Use the standard frame-heap PSSetTextures path for all APIs.
    // Avoids SetDescriptorHeaps thrashing which invalidates root descriptor tables.
    rc.commandList->PSSetTextures(0, _countof(srvs), srvs);
}

void GBufferPBRShader::End(const RenderContext& rc)
{
    ITexture* nullTextures[] = { nullptr, nullptr, nullptr, nullptr };
    rc.commandList->PSSetTextures(0, _countof(nullTextures), nullTextures);
    m_materialOverride = nullptr;
}
