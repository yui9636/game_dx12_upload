#include "SSRPass.h"
#include "Graphics.h"
#include "RHI/ICommandList.h"
#include "RHI/ITexture.h"
#include "RHI/IShader.h"
#include "RHI/IResourceFactory.h"
#include "RHI/PipelineStateDesc.h"
#include "RHI/IPipelineState.h"
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

void SSRPass::Setup(FrameGraphBuilder& builder)
{
    // 1. ���̗͂v��
    m_hGBuffer0 = builder.GetHandle("GBuffer0");
    m_hGBuffer1 = builder.GetHandle("GBuffer1");
    m_hGBuffer2 = builder.GetHandle("GBuffer2");
    m_hPrevScene = builder.GetHandle("PrevScene");

    if (m_hGBuffer0.IsValid())  builder.Read(m_hGBuffer0);
    if (m_hGBuffer1.IsValid())  builder.Read(m_hGBuffer1);
    if (m_hGBuffer2.IsValid())  builder.Read(m_hGBuffer2);
    if (m_hPrevScene.IsValid()) builder.Read(m_hPrevScene);

    // =========================================================
    // �� �C���F�^�̃����_�����O�𑜓x(857x482����)���甼�����v�Z����
    // =========================================================
    float renderScale = Graphics::Instance().GetRenderScale();
    uint32_t renderW = (uint32_t)(Graphics::Instance().GetScreenWidth() * renderScale);
    uint32_t renderH = (uint32_t)(Graphics::Instance().GetScreenHeight() * renderScale);

    // 2. �n�[�t�𑜓x�̐݌v�}
    TextureDesc ssrDesc{};
    ssrDesc.width = renderW / 2;
    ssrDesc.height = renderH / 2;
    ssrDesc.format = TextureFormat::R16G16B16A16_FLOAT;
    ssrDesc.bindFlags = TextureBindFlags::RenderTarget | TextureBindFlags::ShaderResource;

    // 3. ���ԁE�ŏI�o�͂̐���
    m_hSSR = builder.CreateTexture("SSRRaw", ssrDesc);
    m_hSSR = builder.Write(m_hSSR);

    m_hSSRBlur = builder.CreateTexture("SSRBlur", ssrDesc);
    m_hSSRBlur = builder.Write(m_hSSRBlur);

    builder.RegisterHandle("SSRBlur", m_hSSRBlur);
}

void SSRPass::Execute(FrameGraphResources& resources, const RenderQueue& queue, RenderContext& rc)
{
    ITexture* ssrTex = resources.GetTexture(m_hSSR);
    ITexture* blurTex = resources.GetTexture(m_hSSRBlur);
    if (!ssrTex || !blurTex) return;

    rc.commandList->SetVertexBuffer(0, nullptr, 0, 0);
    rc.commandList->SetIndexBuffer(nullptr, IndexFormat::Uint32, 0);

    float clearColor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };

    // =========================================================
    // �� �C���F�r���[�|�[�g�� 857x482 �̔���(428x241)�ɍ��킹��
    // =========================================================
    float halfWidth = (float)ssrTex->GetWidth();
    float halfHeight = (float)ssrTex->GetHeight();

    // ==========================================
    // �p�X1: ���C�}�[�`���O (��SSR����)
    // ==========================================
    rc.commandList->ClearColor(ssrTex, clearColor);
    rc.commandList->SetRenderTarget(ssrTex, nullptr);

    rc.mainRenderTarget = ssrTex;
    rc.mainDepthStencil = nullptr;
    rc.mainViewport = RhiViewport(0.0f, 0.0f, halfWidth, halfHeight);

    rc.commandList->SetPipelineState(m_psoRaymarch.get());
    rc.commandList->PSSetSampler(2, rc.renderState->GetSamplerState(SamplerState::PointClamp));
    rc.commandList->PSSetSampler(3, rc.renderState->GetSamplerState(SamplerState::LinearClamp));

    ITexture* rayTextures[] = {
        resources.GetTexture(m_hGBuffer0),
        resources.GetTexture(m_hGBuffer1),
        resources.GetTexture(m_hGBuffer2),
        resources.GetTexture(m_hPrevScene)
    };
    rc.commandList->PSSetTextures(0, 4, rayTextures);

    rc.commandList->Draw(3, 0);

    // ==========================================
    // �p�X2: ��ԃu���[
    // ==========================================
    rc.commandList->ClearColor(blurTex, clearColor);
    rc.commandList->SetRenderTarget(blurTex, nullptr);

    rc.mainRenderTarget = blurTex;
    rc.mainViewport = RhiViewport(0.0f, 0.0f, halfWidth, halfHeight);

    rc.commandList->SetPipelineState(m_psoBlur.get());

    ITexture* blurInputs[] = {
        ssrTex,
        resources.GetTexture(m_hGBuffer1),
        resources.GetTexture(m_hGBuffer2)
    };
    rc.commandList->PSSetTextures(0, 3, blurInputs);

    rc.commandList->Draw(3, 0);

    ITexture* null4[4] = { nullptr };
    rc.commandList->PSSetTextures(0, 4, null4);
    rc.commandList->PSSetSampler(2, nullptr);
    rc.commandList->PSSetSampler(3, nullptr);
}
