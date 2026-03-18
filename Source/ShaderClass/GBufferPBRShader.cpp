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
        auto* dx12Cmd = static_cast<DX12CommandList*>(rc.commandList);
        if (dx12Cmd) {
            DX12CommandList::PixelTextureBinding bindings[] = {
                { 0, srvs[0], DX12CommandList::NullSrvKind::Texture2D },
                { 1, srvs[1], DX12CommandList::NullSrvKind::Texture2D },
                { 2, srvs[2], DX12CommandList::NullSrvKind::Texture2D },
                { 3, srvs[3], DX12CommandList::NullSrvKind::Texture2D },
            };
            dx12Cmd->BindPixelTextureTable(bindings, _countof(bindings));
            return;
        }
    }

    rc.commandList->PSSetTextures(0, _countof(srvs), srvs);
}

void GBufferPBRShader::End(const RenderContext& rc)
{
    ITexture* nullTextures[] = { nullptr, nullptr, nullptr, nullptr };
    rc.commandList->PSSetTextures(0, _countof(nullTextures), nullTextures);
}
