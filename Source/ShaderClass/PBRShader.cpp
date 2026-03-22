#include "PBRShader.h"

#include "Graphics.h"
#include "ShadowMap.h"
#include "System/ResourceManager.h"
#include "RHI/IResourceFactory.h"
#include "RHI/ICommandList.h"
#include "RHI/ITexture.h"
#include "RHI/IBuffer.h"
#include "RHI/PipelineStateDesc.h"
#include "RHI/IPipelineState.h"
#include "RHI/DX12/DX12CommandList.h"

PBRShader::PBRShader(IResourceFactory* factory)
{
    m_vs = factory->CreateShader(ShaderType::Vertex, "Data/Shader/PBRVS.cso");
    m_ps = factory->CreateShader(ShaderType::Pixel, "Data/Shader/PBRPS.cso");
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

    auto& rm = ResourceManager::Instance();
    m_diffuseIem = rm.GetTexture("Data/Texture/IBL/diffuse_iem.dds");
    m_specularPmrem = rm.GetTexture("Data/Texture/IBL/specular_pmrem.dds");
    m_lutGgx = rm.GetTexture("Data/Texture/IBL/lut_ggx.dds");

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

void PBRShader::Begin(const RenderContext& rc)
{
    rc.commandList->SetPipelineState(m_pso.get());
    if (Graphics::Instance().GetAPI() != GraphicsAPI::DX12) {
        rc.commandList->PSSetConstantBuffer(1, m_meshConstantBuffer.get());
    }
}

void PBRShader::BeginInstanced(const RenderContext& rc)
{
    rc.commandList->SetPipelineState(m_instancedPso ? m_instancedPso.get() : m_pso.get());
    if (Graphics::Instance().GetAPI() != GraphicsAPI::DX12) {
        rc.commandList->PSSetConstantBuffer(1, m_meshConstantBuffer.get());
    }
}

bool PBRShader::SupportsInstancing(const ModelResource::MeshResource& mesh) const
{
    return mesh.bones.empty();
}

void PBRShader::SetMaterialProperties(const DirectX::XMFLOAT4& baseColor, float metallic, float roughness, float emissive)
{
    m_matColor = baseColor;
    m_matMetallic = metallic;
    m_matRoughness = roughness;
    m_matEmissive = emissive;
}

void PBRShader::Update(const RenderContext& rc, const ModelResource::MeshResource& mesh)
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
    cbMesh.occlusionStrength = mesh.material.occlusionStrength;

    if (Graphics::Instance().GetAPI() == GraphicsAPI::DX12) {
        auto* dx12Cmd = static_cast<DX12CommandList*>(rc.commandList);
        dx12Cmd->PSSetDynamicConstantBuffer(1, &cbMesh, sizeof(cbMesh));
    } else {
        rc.commandList->UpdateBuffer(m_meshConstantBuffer.get(), &cbMesh, sizeof(cbMesh));
    }

    ITexture* shadowTex = rc.shadowMap ? rc.shadowMap->GetTexture() : nullptr;
    ITexture* materialTextures[] = {
        mesh.material.albedoMap.get(),
        mesh.material.normalMap.get(),
        mesh.material.metallicMap.get(),
        mesh.material.roughnessMap.get(),
        mesh.material.occlusionMap.get(),
        shadowTex
    };
    rc.commandList->PSSetTextures(0, _countof(materialTextures), materialTextures);
    rc.commandList->PSSetTexture(33, m_diffuseIem.get());
    rc.commandList->PSSetTexture(34, m_specularPmrem.get());
    rc.commandList->PSSetTexture(35, m_lutGgx.get());
}

void PBRShader::End(const RenderContext& rc)
{
    ITexture* nullTextures[] = { nullptr, nullptr, nullptr, nullptr, nullptr, nullptr };
    rc.commandList->PSSetTextures(0, _countof(nullTextures), nullTextures);
    rc.commandList->PSSetTexture(33, nullptr);
    rc.commandList->PSSetTexture(34, nullptr);
    rc.commandList->PSSetTexture(35, nullptr);
}

void PBRShader::SetIBLTextures(ITexture* pDiffuseIEM, ITexture* pSpecularPMREM)
{
    if (pDiffuseIEM) m_diffuseIem = std::shared_ptr<ITexture>(pDiffuseIEM, [](ITexture*) {});
    if (pSpecularPMREM) m_specularPmrem = std::shared_ptr<ITexture>(pSpecularPMREM, [](ITexture*) {});
}
