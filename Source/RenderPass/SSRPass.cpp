#include "SSRPass.h"
#include "Graphics.h"
#include "RHI/ICommandList.h"
#include "RHI/ITexture.h"
#include "RHI/IShader.h"
#include "RHI/IResourceFactory.h"
#include "RHI/PipelineStateDesc.h"
#include "RHI/IPipelineState.h"
#include "RHI/DX12/DX12CommandList.h"
#include "RenderGraph/FrameGraphResources.h"

SSRPass::~SSRPass() = default;

SSRPass::SSRPass(IResourceFactory* factory)
{
    m_vs = factory->CreateShader(ShaderType::Vertex, "Data/Shader/DeferredLightingVS.cso");
    m_psRaymarch = factory->CreateShader(ShaderType::Pixel, "Data/Shader/SSRPS.cso");
    m_psBlur = factory->CreateShader(ShaderType::Pixel, "Data/Shader/SSRBlurPS.cso");

    auto* rs = Graphics::Instance().GetRenderState();
    PipelineStateDesc desc{};
    desc.vertexShader = m_vs.get();
    desc.inputLayout = nullptr;
    desc.depthStencilState = rs->GetDepthStencilState(DepthState::NoTestNoWrite);
    desc.rasterizerState = rs->GetRasterizerState(RasterizerState::SolidCullNone);
    desc.blendState = rs->GetBlendState(BlendState::Opaque);
    desc.primitiveTopology = PrimitiveTopology::TriangleList;
    desc.numRenderTargets = 1;
    desc.rtvFormats[0] = TextureFormat::R16G16B16A16_FLOAT;
    desc.dsvFormat = TextureFormat::Unknown;

    desc.pixelShader = m_psRaymarch.get();
    m_psoRaymarch = factory->CreatePipelineState(desc);

    desc.pixelShader = m_psBlur.get();
    m_psoBlur = factory->CreatePipelineState(desc);
}

void SSRPass::Setup(FrameGraphBuilder& builder, const RenderContext& rc)
{
    m_hGBuffer0 = builder.GetHandle("GBuffer0");
    m_hGBuffer1 = builder.GetHandle("GBuffer1");
    m_hGBuffer2 = builder.GetHandle("GBuffer2");
    m_hPrevScene = builder.GetHandle("PrevScene");

    if (m_hGBuffer0.IsValid())  builder.Read(m_hGBuffer0);
    if (m_hGBuffer1.IsValid())  builder.Read(m_hGBuffer1);
    if (m_hGBuffer2.IsValid())  builder.Read(m_hGBuffer2);
    if (m_hPrevScene.IsValid()) builder.Read(m_hPrevScene);

    if (!rc.enableSSR) {
        m_hSSR = {};
        m_hSSRBlur = {};
        return;
    }

    // =========================================================
    // =========================================================
    uint32_t renderW = rc.renderWidth;
    uint32_t renderH = rc.renderHeight;
    if (renderW == 0 || renderH == 0) {
        float renderScale = Graphics::Instance().GetRenderScale();
        renderW = (uint32_t)(Graphics::Instance().GetScreenWidth() * renderScale);
        renderH = (uint32_t)(Graphics::Instance().GetScreenHeight() * renderScale);
    }

    TextureDesc ssrDesc{};
    ssrDesc.width = renderW / 2;
    ssrDesc.height = renderH / 2;
    ssrDesc.format = TextureFormat::R16G16B16A16_FLOAT;
    ssrDesc.bindFlags = TextureBindFlags::RenderTarget | TextureBindFlags::ShaderResource;

    m_hSSR = builder.CreateTexture("SSRRaw", ssrDesc);
    m_hSSR = builder.Write(m_hSSR);

    m_hSSRBlur = builder.CreateTexture("SSRBlur", ssrDesc);
    m_hSSRBlur = builder.Write(m_hSSRBlur);

    builder.RegisterHandle("SSRBlur", m_hSSRBlur);
}

void SSRPass::Execute(FrameGraphResources& resources, const RenderQueue& queue, RenderContext& rc)
{
    if (!rc.enableSSR) {
        return;
    }

    ITexture* ssrTex = resources.GetTexture(m_hSSR);
    ITexture* blurTex = resources.GetTexture(m_hSSRBlur);
    if (!ssrTex || !blurTex) return;

    rc.commandList->SetVertexBuffer(0, nullptr, 0, 0);
    rc.commandList->SetIndexBuffer(nullptr, IndexFormat::Uint32, 0);

    float clearColor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };

    // =========================================================
    // =========================================================
    float halfWidth = (float)ssrTex->GetWidth();
    float halfHeight = (float)ssrTex->GetHeight();

    // ==========================================
    // ==========================================
    rc.commandList->ClearColor(ssrTex, clearColor);
    rc.commandList->SetRenderTarget(ssrTex, nullptr);

    rc.mainRenderTarget = ssrTex;
    rc.mainDepthStencil = nullptr;
    rc.mainViewport = RhiViewport(0.0f, 0.0f, halfWidth, halfHeight);
    rc.commandList->SetViewport(rc.mainViewport);

    rc.commandList->SetPipelineState(m_psoRaymarch.get());
    rc.commandList->PSSetSampler(2, rc.renderState->GetSamplerState(SamplerState::PointClamp));
    rc.commandList->PSSetSampler(3, rc.renderState->GetSamplerState(SamplerState::LinearClamp));

    ITexture* rayTextures[] = {
        resources.GetTexture(m_hGBuffer0),
        resources.GetTexture(m_hGBuffer1),
        resources.GetTexture(m_hGBuffer2),
        resources.GetTexture(m_hPrevScene)
    };
    if (Graphics::Instance().GetAPI() == GraphicsAPI::DX12) {
        auto* dx12Cmd = static_cast<DX12CommandList*>(rc.commandList);
        DX12CommandList::PixelTextureBinding bindings[] = {
            { 0, rayTextures[0], DX12CommandList::NullSrvKind::Texture2D },
            { 1, rayTextures[1], DX12CommandList::NullSrvKind::Texture2D },
            { 2, rayTextures[2], DX12CommandList::NullSrvKind::Texture2D },
            { 3, rayTextures[3], DX12CommandList::NullSrvKind::Texture2D },
        };
        dx12Cmd->BindPixelTextureTable(bindings, _countof(bindings));
    }
    else {
        rc.commandList->PSSetTextures(0, 4, rayTextures);
    }

    rc.commandList->Draw(3, 0);

    // ==========================================
    // ==========================================
    rc.commandList->ClearColor(blurTex, clearColor);
    rc.commandList->SetRenderTarget(blurTex, nullptr);

    rc.mainRenderTarget = blurTex;
    rc.mainViewport = RhiViewport(0.0f, 0.0f, halfWidth, halfHeight);
    rc.commandList->SetViewport(rc.mainViewport);

    rc.commandList->SetPipelineState(m_psoBlur.get());

    ITexture* blurInputs[] = {
        ssrTex,
        resources.GetTexture(m_hGBuffer1),
        resources.GetTexture(m_hGBuffer2)
    };
    if (Graphics::Instance().GetAPI() == GraphicsAPI::DX12) {
        auto* dx12Cmd = static_cast<DX12CommandList*>(rc.commandList);
        DX12CommandList::PixelTextureBinding bindings[] = {
            { 0, blurInputs[0], DX12CommandList::NullSrvKind::Texture2D },
            { 1, blurInputs[1], DX12CommandList::NullSrvKind::Texture2D },
            { 2, blurInputs[2], DX12CommandList::NullSrvKind::Texture2D },
        };
        dx12Cmd->BindPixelTextureTable(bindings, _countof(bindings));
    }
    else {
        rc.commandList->PSSetTextures(0, 3, blurInputs);
    }

    rc.commandList->Draw(3, 0);

    if (Graphics::Instance().GetAPI() != GraphicsAPI::DX12) {
        ITexture* null4[4] = { nullptr };
        rc.commandList->PSSetTextures(0, 4, null4);
        rc.commandList->PSSetSampler(2, nullptr);
        rc.commandList->PSSetSampler(3, nullptr);
    }
}
