#include "GTAOPass.h"
#include "Graphics.h"
#include "RHI/ICommandList.h"
#include "RHI/ITexture.h"
#include "RHI/IShader.h"
#include "RHI/IResourceFactory.h"
#include "RHI/PipelineStateDesc.h"
#include "RHI/IPipelineState.h"
#include "RHI/DX12/DX12CommandList.h"
#include "RenderGraph/FrameGraphResources.h"

GTAOPass::~GTAOPass() = default;

GTAOPass::GTAOPass(IResourceFactory* factory)
{
    m_vs = factory->CreateShader(ShaderType::Vertex, "Data/Shader/DeferredLightingVS.cso");
    m_ps = factory->CreateShader(ShaderType::Pixel, "Data/Shader/GTAOPS.cso");

    auto* rs = Graphics::Instance().GetRenderState();

    PipelineStateDesc desc{};
    desc.vertexShader = m_vs.get();
    desc.pixelShader = m_ps.get();
    desc.inputLayout = nullptr;
    desc.depthStencilState = rs->GetDepthStencilState(DepthState::NoTestNoWrite);
    desc.rasterizerState = rs->GetRasterizerState(RasterizerState::SolidCullNone);
    desc.blendState = rs->GetBlendState(BlendState::Opaque);
    desc.primitiveTopology = PrimitiveTopology::TriangleList;

    desc.numRenderTargets = 1;
    desc.rtvFormats[0] = TextureFormat::R8_UNORM;
    desc.dsvFormat = TextureFormat::Unknown;

    m_pso = factory->CreatePipelineState(desc);
}

void GTAOPass::Setup(FrameGraphBuilder& builder)
{
    m_hGBuffer1 = builder.GetHandle("GBuffer1");
    m_hGBuffer2 = builder.GetHandle("GBuffer2");

    if (m_hGBuffer1.IsValid()) builder.Read(m_hGBuffer1);
    if (m_hGBuffer2.IsValid()) builder.Read(m_hGBuffer2);

    // =========================================================
    // �� �C���F�^�̃����_�����O�𑜓x(857x482����)�ɍ��킹��
    // =========================================================
    float renderScale = Graphics::Instance().GetRenderScale();
    uint32_t renderW = (uint32_t)(Graphics::Instance().GetScreenWidth() * renderScale);
    uint32_t renderH = (uint32_t)(Graphics::Instance().GetScreenHeight() * renderScale);

    TextureDesc desc{};
    desc.width = renderW;
    desc.height = renderH;
    desc.format = TextureFormat::R8_UNORM;
    desc.bindFlags = TextureBindFlags::RenderTarget | TextureBindFlags::ShaderResource;

    m_hGTAO = builder.CreateTexture("GTAO", desc);
    m_hGTAO = builder.Write(m_hGTAO);

    builder.RegisterHandle("GTAO", m_hGTAO);
}

void GTAOPass::Execute(FrameGraphResources& resources, const RenderQueue& queue, RenderContext& rc)
{
    ITexture* gtaoTex = resources.GetTexture(m_hGTAO);
    ITexture* gbuffer1 = resources.GetTexture(m_hGBuffer1);
    ITexture* gbuffer2 = resources.GetTexture(m_hGBuffer2);

    if (!gtaoTex || !gbuffer1 || !gbuffer2) return;

    rc.commandList->SetVertexBuffer(0, nullptr, 0, 0);
    rc.commandList->SetIndexBuffer(nullptr, IndexFormat::Uint32, 0);

    float clearColor[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
    rc.commandList->ClearColor(gtaoTex, clearColor);
    rc.commandList->SetRenderTarget(gtaoTex, nullptr);

    // =========================================================
    // �� �C���F�e�N�X�`�����g�̃T�C�Y(857x482����)���r���[�|�[�g�Ɏg�p
    // =========================================================
    rc.mainRenderTarget = gtaoTex;
    rc.mainDepthStencil = nullptr;
    rc.mainViewport = RhiViewport(0.0f, 0.0f, (float)gtaoTex->GetWidth(), (float)gtaoTex->GetHeight());
    rc.commandList->SetViewport(rc.mainViewport);

    rc.commandList->SetPipelineState(m_pso.get());

    if (Graphics::Instance().GetAPI() == GraphicsAPI::DX12) {
        auto* dx12Cmd = static_cast<DX12CommandList*>(rc.commandList);
        DX12CommandList::PixelTextureBinding bindings[] = {
            { 0, gbuffer1, DX12CommandList::NullSrvKind::Texture2D },
            { 1, gbuffer2, DX12CommandList::NullSrvKind::Texture2D },
        };
        dx12Cmd->BindPixelTextureTable(bindings, _countof(bindings));
    }
    else {
        ITexture* inputs[] = { gbuffer1, gbuffer2 };
        rc.commandList->PSSetTextures(0, 2, inputs);
    }

    auto* pointSampler = rc.renderState->GetSamplerState(SamplerState::PointClamp);
    rc.commandList->PSSetSampler(2, pointSampler);

    rc.commandList->Draw(3, 0);

    if (Graphics::Instance().GetAPI() != GraphicsAPI::DX12) {
        rc.commandList->PSSetSampler(2, nullptr);
        ITexture* nullTextures[2] = { nullptr };
        rc.commandList->PSSetTextures(0, 2, nullTextures);
    }
}
