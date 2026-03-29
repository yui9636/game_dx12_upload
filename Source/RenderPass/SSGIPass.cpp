#include "SSGIPass.h"
#include "Graphics.h"
#include "RHI/ICommandList.h"
#include "RHI/ITexture.h"
#include "RHI/IShader.h"
#include "RHI/IResourceFactory.h"
#include "RHI/PipelineStateDesc.h"
#include "RHI/IPipelineState.h"
#include "RHI/DX12/DX12CommandList.h"
#include "RenderGraph/FrameGraphResources.h"

SSGIPass::~SSGIPass() = default;

SSGIPass::SSGIPass(IResourceFactory* factory)
{
    m_vs = factory->CreateShader(ShaderType::Vertex, "Data/Shader/DeferredLightingVS.cso");
    m_psRaymarch = factory->CreateShader(ShaderType::Pixel, "Data/Shader/SSGIPS.cso");
    m_psBlur = factory->CreateShader(ShaderType::Pixel, "Data/Shader/SSGIBlurPS.cso");

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

void SSGIPass::Setup(FrameGraphBuilder& builder, const RenderContext& rc)
{
    m_hGBuffer1 = builder.GetHandle("GBuffer1");
    m_hGBuffer2 = builder.GetHandle("GBuffer2");
    m_hPrevScene = builder.GetHandle("PrevScene");

    if (m_hGBuffer1.IsValid())  builder.Read(m_hGBuffer1);
    if (m_hGBuffer2.IsValid())  builder.Read(m_hGBuffer2);
    if (m_hPrevScene.IsValid()) builder.Read(m_hPrevScene);

    if (!rc.enableSSGI) {
        m_hSSGI = {};
        m_hSSGIBlur = {};
        return;
    }

    // =========================================================
    // ★ 修正：真のレンダリング解像度(857x482相当)から半分を計算する
    // =========================================================
    uint32_t renderW = rc.renderWidth;
    uint32_t renderH = rc.renderHeight;
    if (renderW == 0 || renderH == 0) {
        float renderScale = Graphics::Instance().GetRenderScale();
        renderW = (uint32_t)(Graphics::Instance().GetScreenWidth() * renderScale);
        renderH = (uint32_t)(Graphics::Instance().GetScreenHeight() * renderScale);
    }

    TextureDesc ssgiDesc{};
    ssgiDesc.width = renderW / 2;
    ssgiDesc.height = renderH / 2;
    ssgiDesc.format = TextureFormat::R16G16B16A16_FLOAT;
    ssgiDesc.bindFlags = TextureBindFlags::RenderTarget | TextureBindFlags::ShaderResource;

    m_hSSGI = builder.CreateTexture("SSGIRaw", ssgiDesc);
    m_hSSGI = builder.Write(m_hSSGI);

    m_hSSGIBlur = builder.CreateTexture("SSGIBlur", ssgiDesc);
    m_hSSGIBlur = builder.Write(m_hSSGIBlur);

    builder.RegisterHandle("SSGIBlur", m_hSSGIBlur);
}

void SSGIPass::Execute(FrameGraphResources& resources, const RenderQueue& queue, RenderContext& rc)
{
    if (!rc.enableSSGI) {
        return;
    }

    ITexture* ssgiTex = resources.GetTexture(m_hSSGI);
    ITexture* blurTex = resources.GetTexture(m_hSSGIBlur);
    if (!ssgiTex || !blurTex) return;

    rc.commandList->SetVertexBuffer(0, nullptr, 0, 0);
    rc.commandList->SetIndexBuffer(nullptr, IndexFormat::Uint32, 0);

    float clearColor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };

    // =========================================================
    // ★ 修正：ビューポートも 857x482 の半分(428x241)に合わせる
    // =========================================================
    float halfWidth = (float)ssgiTex->GetWidth();
    float halfHeight = (float)ssgiTex->GetHeight();

    // パス1: レイマーチング
    rc.commandList->ClearColor(ssgiTex, clearColor);
    rc.commandList->SetRenderTarget(ssgiTex, nullptr);

    rc.mainRenderTarget = ssgiTex;
    rc.mainDepthStencil = nullptr;
    rc.mainViewport = RhiViewport(0.0f, 0.0f, halfWidth, halfHeight);
    rc.commandList->SetViewport(rc.mainViewport);

    rc.commandList->SetPipelineState(m_psoRaymarch.get());
    rc.commandList->PSSetSampler(2, rc.renderState->GetSamplerState(SamplerState::PointClamp));
    rc.commandList->PSSetSampler(3, rc.renderState->GetSamplerState(SamplerState::LinearClamp));

    ITexture* rayInputs[] = {
        resources.GetTexture(m_hGBuffer1),
        resources.GetTexture(m_hGBuffer2),
        resources.GetTexture(m_hPrevScene)
    };
    if (Graphics::Instance().GetAPI() == GraphicsAPI::DX12) {
        auto* dx12Cmd = static_cast<DX12CommandList*>(rc.commandList);
        DX12CommandList::PixelTextureBinding bindings[] = {
            { 0, rayInputs[0], DX12CommandList::NullSrvKind::Texture2D },
            { 1, rayInputs[1], DX12CommandList::NullSrvKind::Texture2D },
            { 2, rayInputs[2], DX12CommandList::NullSrvKind::Texture2D },
        };
        dx12Cmd->BindPixelTextureTable(bindings, _countof(bindings));
    }
    else {
        rc.commandList->PSSetTextures(0, 3, rayInputs);
    }

    rc.commandList->Draw(3, 0);

    // パス2: 空間ブラー
    rc.commandList->ClearColor(blurTex, clearColor);
    rc.commandList->SetRenderTarget(blurTex, nullptr);

    rc.mainRenderTarget = blurTex;
    rc.mainViewport = RhiViewport(0.0f, 0.0f, halfWidth, halfHeight);
    rc.commandList->SetViewport(rc.mainViewport);

    rc.commandList->SetPipelineState(m_psoBlur.get());

    ITexture* blurInputs[] = {
        ssgiTex,
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
        ITexture* null3[3] = { nullptr };
        rc.commandList->PSSetTextures(0, 3, null3);
        rc.commandList->PSSetSampler(2, nullptr);
        rc.commandList->PSSetSampler(3, nullptr);
    }
}
