#include "ToonShader.h"

#include "Graphics.h"
#include "ShadowMap.h"
#include "Material/MaterialAsset.h"
#include "System/ResourceManager.h"
#include "RHI/IResourceFactory.h"
#include "RHI/ICommandList.h"
#include "RHI/ITexture.h"
#include "RHI/IBuffer.h"
#include "RHI/PipelineStateDesc.h"
#include "RHI/IPipelineState.h"
#include "RHI/DX12/DX12CommandList.h"
#include "Model/ModelResource.h"
#include "RenderContext/RenderState.h"
#include <DirectXTex.h>

ToonShader::ToonShader(IResourceFactory* factory)
{
    m_vs        = factory->CreateShader(ShaderType::Vertex, "Data/Shader/ToonVS.cso");
    m_ps        = factory->CreateShader(ShaderType::Pixel,  "Data/Shader/ToonPS.cso");
    m_outlineVs = factory->CreateShader(ShaderType::Vertex, "Data/Shader/ToonOutlineVS.cso");
    m_outlinePs = factory->CreateShader(ShaderType::Pixel,  "Data/Shader/ToonOutlinePS.cso");

    InputLayoutElement layout[] = {
        {"POSITION",     0, TextureFormat::R32G32B32_FLOAT,    0, kAppendAlignedElement},
        {"BONE_WEIGHTS", 0, TextureFormat::R32G32B32A32_FLOAT, 0, kAppendAlignedElement},
        {"BONE_INDICES", 0, TextureFormat::R32G32B32A32_UINT,  0, kAppendAlignedElement},
        {"TEXCOORD",     0, TextureFormat::R32G32_FLOAT,       0, kAppendAlignedElement},
        {"NORMAL",       0, TextureFormat::R32G32B32_FLOAT,    0, kAppendAlignedElement},
        {"TANGENT",      0, TextureFormat::R32G32B32_FLOAT,    0, kAppendAlignedElement},
    };
    InputLayoutDesc layoutDesc = { layout, _countof(layout) };
    m_inputLayout = factory->CreateInputLayout(layoutDesc, m_vs.get());

    m_meshConstantBuffer = factory->CreateBuffer(sizeof(CbMesh), BufferType::Constant);
    m_toonConstantBuffer = factory->CreateBuffer(sizeof(CbToon), BufferType::Constant);

    // Body PSO (cull back, depth test+write)
    PipelineStateDesc desc{};
    desc.vertexShader = m_vs.get();
    desc.pixelShader  = m_ps.get();
    desc.inputLayout  = m_inputLayout.get();
    desc.primitiveTopology = PrimitiveTopology::TriangleList;
    desc.numRenderTargets  = 1;
    desc.rtvFormats[0] = TextureFormat::R16G16B16A16_FLOAT;
    desc.dsvFormat     = TextureFormat::D32_FLOAT;
    m_pso = factory->CreatePipelineState(desc);

    // Outline PSO (cull front, expanded backfaces become silhouette)
    PipelineStateDesc outlineDesc = desc;
    outlineDesc.vertexShader = m_outlineVs.get();
    outlineDesc.pixelShader  = m_outlinePs.get();
    m_outlinePso = factory->CreatePipelineState(outlineDesc);

    // 1x1 white texture (diffuse / ramp fallback)
    {
        DirectX::ScratchImage img;
        img.Initialize2D(DXGI_FORMAT_R8G8B8A8_UNORM, 1, 1, 1, 1);
        uint8_t* px = img.GetPixels();
        px[0] = 255; px[1] = 255; px[2] = 255; px[3] = 255;
        auto tex = factory->CreateTextureFromMemory(img, img.GetMetadata());
        if (tex) m_whiteTexture = std::shared_ptr<ITexture>(tex.release());
    }
    // Neutral normal (0.5, 0.5, 1.0)
    {
        DirectX::ScratchImage img;
        img.Initialize2D(DXGI_FORMAT_R8G8B8A8_UNORM, 1, 1, 1, 1);
        uint8_t* px = img.GetPixels();
        px[0] = 128; px[1] = 128; px[2] = 255; px[3] = 255;
        auto tex = factory->CreateTextureFromMemory(img, img.GetMetadata());
        if (tex) m_neutralNormal = std::shared_ptr<ITexture>(tex.release());
    }
    // Default ramp (4x1 step gradient)
    {
        DirectX::ScratchImage img;
        img.Initialize2D(DXGI_FORMAT_R8G8B8A8_UNORM, 4, 1, 1, 1);
        uint8_t* px = img.GetPixels();
        px[ 0] =  40; px[ 1] =  40; px[ 2] =  40; px[ 3] = 255;
        px[ 4] = 128; px[ 5] = 128; px[ 6] = 128; px[ 7] = 255;
        px[ 8] = 220; px[ 9] = 220; px[10] = 220; px[11] = 255;
        px[12] = 255; px[13] = 255; px[14] = 255; px[15] = 255;
        auto tex = factory->CreateTextureFromMemory(img, img.GetMetadata());
        if (tex) m_defaultRamp = std::shared_ptr<ITexture>(tex.release());
    }
}

void ToonShader::Begin(const RenderContext& rc)
{
    if (Graphics::Instance().GetAPI() != GraphicsAPI::DX12) {
        rc.commandList->PSSetConstantBuffer(1, m_meshConstantBuffer.get());
        rc.commandList->PSSetConstantBuffer(2, m_toonConstantBuffer.get());
        rc.commandList->VSSetConstantBuffer(2, m_toonConstantBuffer.get());
    }
}

void ToonShader::BeginInstanced(const RenderContext& rc)
{
    Begin(rc);
}

bool ToonShader::SupportsInstancing(const ModelResource::MeshResource& /*mesh*/) const
{
    return false;
}

void ToonShader::Update(const RenderContext& rc, const ModelResource::MeshResource& mesh)
{
    using namespace DirectX;
    const bool isDX12 = Graphics::Instance().GetAPI() == GraphicsAPI::DX12;

    CbMesh cbMesh{};
    cbMesh.materialColor = mesh.material.color;

    CbToon cbToon{};
    if (m_materialOverride) {
        const auto& m = *m_materialOverride;
        cbToon.shadowMid    = { m.toonShadowTint.x, m.toonShadowTint.y, m.toonShadowTint.z, 0.0f };
        cbToon.shadowDeep   = { m.toonShadowDeep.x, m.toonShadowDeep.y, m.toonShadowDeep.z, 0.0f };
        cbToon.shadowParams = { m.toonShadowMidThreshold,
                                m.toonShadowDeepThreshold,
                                static_cast<float>(m.toonBandLevels),
                                static_cast<float>(m.toonShadingMode) };
        cbToon.rimColor     = { m.toonRimColor.x, m.toonRimColor.y, m.toonRimColor.z, m.toonRimPower };
        cbToon.rimAux       = { m.toonRimStrength, m.toonOutlineDistanceScale, 0.0f, 0.0f };
        cbToon.outlineColor = { m.toonOutlineColor.x, m.toonOutlineColor.y, m.toonOutlineColor.z, m.toonOutlineWidth };
        cbToon.specColor    = { m.toonSpecColor.x, m.toonSpecColor.y, m.toonSpecColor.z, m.toonSpecStrength };
        cbToon.specParams   = { m.toonSpecSharpness, m.toonSpecThreshold,
                                m.toonUseFixedLight ? 1.0f : 0.0f,
                                m.toonUseAnisotropic ? 1.0f : 0.0f };
        cbToon.fixedLight   = { m.toonFixedLightDir.x, m.toonFixedLightDir.y, m.toonFixedLightDir.z, m.toonAnisoOffset };
        cbToon.anisoAux     = { m.toonAnisoSharpness, 0.0f, 0.0f, 0.0f };
    } else {
        cbToon.shadowMid    = { 0.55f, 0.50f, 0.65f, 0.0f };
        cbToon.shadowDeep   = { 0.25f, 0.22f, 0.38f, 0.0f };
        cbToon.shadowParams = { 0.55f, 0.28f, 3.0f, 0.0f };
        cbToon.rimColor     = { 1.0f, 1.0f, 1.0f, 4.0f };
        cbToon.rimAux       = { 0.4f, 0.0f, 0.0f, 0.0f };
        cbToon.outlineColor = { 0.05f, 0.05f, 0.08f, 0.012f };
        cbToon.specColor    = { 1.0f, 1.0f, 1.0f, 0.0f };
        cbToon.specParams   = { 0.5f, 0.5f, 0.0f, 0.0f };
        cbToon.fixedLight   = { 0.0f, -0.5f, -0.7f, 0.0f };
        cbToon.anisoAux     = { 0.5f, 0.0f, 0.0f, 0.0f };
    }

    if (isDX12) {
        auto* dx12 = static_cast<DX12CommandList*>(rc.commandList);
        dx12->PSSetDynamicConstantBuffer(1, &cbMesh, sizeof(cbMesh));
        dx12->PSSetDynamicConstantBuffer(2, &cbToon, sizeof(cbToon));
        dx12->VSSetDynamicConstantBuffer(2, &cbToon, sizeof(cbToon));
    } else {
        rc.commandList->UpdateBuffer(m_meshConstantBuffer.get(), &cbMesh, sizeof(cbMesh));
        rc.commandList->UpdateBuffer(m_toonConstantBuffer.get(), &cbToon, sizeof(cbToon));
    }

    auto resolveTexture = [&](const std::string& path, std::shared_ptr<ITexture> fallback) -> ITexture* {
        if (m_materialOverride && !path.empty()) {
            if (auto t = ResourceManager::Instance().GetTexture(path)) return t.get();
        }
        return fallback.get();
    };

    ITexture* diffuse = resolveTexture(
        m_materialOverride ? m_materialOverride->diffuseTexturePath : std::string(),
        mesh.material.diffuseMap);
    if (!diffuse) diffuse = m_whiteTexture.get();

    ITexture* normal = resolveTexture(
        m_materialOverride ? m_materialOverride->normalTexturePath : std::string(),
        mesh.material.normalMap);
    if (!normal) normal = m_neutralNormal.get();

    ITexture* shadowTex = rc.shadowMap ? rc.shadowMap->GetTexture() : nullptr;

    ITexture* ramp = m_defaultRamp.get();
    if (m_materialOverride && m_materialOverride->toonUseRampTexture && !m_materialOverride->toonRampTexturePath.empty()) {
        if (auto t = ResourceManager::Instance().GetTexture(m_materialOverride->toonRampTexturePath)) {
            ramp = t.get();
        }
    }

    // Outline pass: draw expanded backfaces in flat color first.
    const bool outlineOn = m_materialOverride ? m_materialOverride->toonOutlineEnabled : true;
    if (outlineOn && cbToon.outlineColor.w > 0.0001f) {
        rc.commandList->SetPipelineState(m_outlinePso.get());
        rc.commandList->DrawIndexed(mesh.indexCount, 0, 0);
    }

    // Body pass: caller (ModelRenderer) will invoke DrawIndexed after this Update.
    rc.commandList->SetPipelineState(m_pso.get());
    ITexture* srvs[] = { diffuse, normal, shadowTex, ramp };
    rc.commandList->PSSetTextures(0, _countof(srvs), srvs);
}

void ToonShader::End(const RenderContext& rc)
{
    ITexture* nulls[] = { nullptr, nullptr, nullptr, nullptr };
    rc.commandList->PSSetTextures(0, _countof(nulls), nulls);
    m_materialOverride = nullptr;
}
