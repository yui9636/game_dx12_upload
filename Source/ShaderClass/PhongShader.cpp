#include "PhongShader.h"

#include "Graphics.h"
#include "ShadowMap.h"
#include "RHI/IResourceFactory.h"
#include "RHI/ICommandList.h"
#include "RHI/ITexture.h"
#include "RHI/IBuffer.h"
#include "RHI/PipelineStateDesc.h"
#include "RHI/IPipelineState.h"
#include "RHI/DX12/DX12CommandList.h"

PhongShader::PhongShader(IResourceFactory* factory)
{
    m_vs = factory->CreateShader(ShaderType::Vertex, "Data/Shader/PhongVS.cso");
    m_ps = factory->CreateShader(ShaderType::Pixel, "Data/Shader/PhongPS.cso");
    m_instancedVs = factory->CreateShader(ShaderType::Vertex, "Data/Shader/PhongInstancedVS.cso");

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
    };
    InputLayoutDesc instancedLayoutDesc = { instancedLayoutElements, _countof(instancedLayoutElements) };
    m_instancedInputLayout = factory->CreateInputLayout(instancedLayoutDesc, m_instancedVs.get());

    m_meshConstantBuffer = factory->CreateBuffer(sizeof(CbMesh), BufferType::Constant);

    PipelineStateDesc desc{};
    desc.vertexShader = m_vs.get();
    desc.pixelShader = m_ps.get();
    desc.inputLayout = m_inputLayout.get();
    desc.primitiveTopology = PrimitiveTopology::TriangleList;
    desc.numRenderTargets = 1;
    desc.rtvFormats[0] = TextureFormat::R16G16B16A16_FLOAT;
    desc.dsvFormat = TextureFormat::D32_FLOAT;
    m_pso = factory->CreatePipelineState(desc);

    PipelineStateDesc instancedDesc = desc;
    instancedDesc.vertexShader = m_instancedVs.get();
    instancedDesc.inputLayout = m_instancedInputLayout.get();
    m_instancedPso = factory->CreatePipelineState(instancedDesc);
}

void PhongShader::Begin(const RenderContext& rc)
{
    rc.commandList->SetPipelineState(m_pso.get());
    if (Graphics::Instance().GetAPI() != GraphicsAPI::DX12) {
        rc.commandList->PSSetConstantBuffer(1, m_meshConstantBuffer.get());
    }
}

void PhongShader::BeginInstanced(const RenderContext& rc)
{
    rc.commandList->SetPipelineState(m_instancedPso ? m_instancedPso.get() : m_pso.get());
    if (Graphics::Instance().GetAPI() != GraphicsAPI::DX12) {
        rc.commandList->PSSetConstantBuffer(1, m_meshConstantBuffer.get());
    }
}

bool PhongShader::SupportsInstancing(const ModelResource::MeshResource& mesh) const
{
    return mesh.bones.empty();
}

void PhongShader::Update(const RenderContext& rc, const ModelResource::MeshResource& mesh)
{
    CbMesh cbMesh{};
    cbMesh.materialColor = mesh.material.color;
    if (Graphics::Instance().GetAPI() == GraphicsAPI::DX12) {
        auto* dx12Cmd = static_cast<DX12CommandList*>(rc.commandList);
        dx12Cmd->PSSetDynamicConstantBuffer(1, &cbMesh, sizeof(cbMesh));
    } else {
        rc.commandList->UpdateBuffer(m_meshConstantBuffer.get(), &cbMesh, sizeof(cbMesh));
    }

    ITexture* shadowTex = rc.shadowMap ? rc.shadowMap->GetTexture() : nullptr;
    ITexture* srvs[] = {
        mesh.material.diffuseMap.get(),
        mesh.material.normalMap.get(),
        shadowTex
    };
    rc.commandList->PSSetTextures(0, _countof(srvs), srvs);
}

void PhongShader::End(const RenderContext& rc)
{
    ITexture* nullTextures[] = { nullptr, nullptr, nullptr };
    rc.commandList->PSSetTextures(0, _countof(nullTextures), nullTextures);
}
