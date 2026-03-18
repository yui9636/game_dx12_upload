#include "VolumetricFogPass.h"
#include "Graphics.h"
#include "RHI/ICommandList.h"
#include "RHI/ITexture.h"
#include "RHI/IShader.h"
#include "RHI/IResourceFactory.h"
#include "RHI/PipelineStateDesc.h"
#include "RHI/IPipelineState.h"
#include "RenderGraph/FrameGraphResources.h"

VolumetricFogPass::~VolumetricFogPass() = default;

VolumetricFogPass::VolumetricFogPass(IResourceFactory* factory)
{
    m_vs = factory->CreateShader(ShaderType::Vertex, "Data/Shader/DeferredLightingVS.cso");
    m_psRaymarch = factory->CreateShader(ShaderType::Pixel, "Data/Shader/VolumetricFogPS.cso");
    m_psBlur = factory->CreateShader(ShaderType::Pixel, "Data/Shader/VolumetricFogBlurPS.cso");

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

void VolumetricFogPass::Setup(FrameGraphBuilder& builder)
{
    // 1. 入力の要求
    m_hGBuffer2 = builder.GetHandle("GBuffer2");
    if (m_hGBuffer2.IsValid()) builder.Read(m_hGBuffer2);

    // =========================================================
    // ★ 修正：真のレンダリング解像度(857x482相当)から半分を計算する
    // =========================================================
    float renderScale = Graphics::Instance().GetRenderScale();
    uint32_t renderW = (uint32_t)(Graphics::Instance().GetScreenWidth() * renderScale);
    uint32_t renderH = (uint32_t)(Graphics::Instance().GetScreenHeight() * renderScale);

    // 2. ハーフ解像度のテクスチャ設計図
    TextureDesc fogDesc{};
    fogDesc.width = renderW / 2;
    fogDesc.height = renderH / 2;
    fogDesc.format = TextureFormat::R16G16B16A16_FLOAT;
    fogDesc.bindFlags = TextureBindFlags::RenderTarget | TextureBindFlags::ShaderResource;

    // 3. パス1の出力
    m_hVolumetricFog = builder.CreateTexture("VolumetricFog", fogDesc);
    m_hVolumetricFog = builder.Write(m_hVolumetricFog);

    // 4. パス2の出力（最終結果）
    m_hVolumetricFogBlur = builder.CreateTexture("VolumetricFogBlur", fogDesc);
    m_hVolumetricFogBlur = builder.Write(m_hVolumetricFogBlur);

    builder.RegisterHandle("VolumetricFogBlur", m_hVolumetricFogBlur);
}

void VolumetricFogPass::Execute(FrameGraphResources& resources, const RenderQueue& queue, RenderContext& rc)
{
    ITexture* gbuffer2 = resources.GetTexture(m_hGBuffer2);
    ITexture* fogTex = resources.GetTexture(m_hVolumetricFog);
    ITexture* blurTex = resources.GetTexture(m_hVolumetricFogBlur);

    if (!gbuffer2 || !fogTex || !blurTex) return;

    rc.commandList->SetVertexBuffer(0, nullptr, 0, 0);
    rc.commandList->SetIndexBuffer(nullptr, IndexFormat::Uint32, 0);

    float clearColor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };

    // =========================================================
    // ★ 修正：実際のテクスチャ解像度(428x241相当)をビューポートに使用する
    // =========================================================
    float halfWidth = (float)fogTex->GetWidth();
    float halfHeight = (float)fogTex->GetHeight();

    // ==========================================
    // パス1: レイマーチング (生フォグ生成)
    // ==========================================
    rc.commandList->ClearColor(fogTex, clearColor);
    rc.commandList->SetRenderTargets(1, &fogTex, nullptr);

    rc.mainRenderTarget = fogTex;
    rc.mainDepthStencil = nullptr;
    rc.mainViewport = RhiViewport(0.0f, 0.0f, halfWidth, halfHeight);

    rc.commandList->SetPipelineState(m_psoRaymarch.get());
    rc.commandList->PSSetSampler(2, rc.renderState->GetSamplerState(SamplerState::PointClamp));
    if (rc.shadowMap) {
        rc.commandList->PSSetSampler(1, rc.shadowMap->GetSamplerState());
    }
    rc.commandList->PSSetTexture(0, gbuffer2);
    rc.commandList->Draw(3, 0);

    // ==========================================
    // パス2: 空間ブラー (バンディングノイズ除去)
    // ==========================================
    rc.commandList->ClearColor(blurTex, clearColor);
    rc.commandList->SetRenderTargets(1, &blurTex, nullptr);

    rc.mainRenderTarget = blurTex;
    rc.mainViewport = RhiViewport(0.0f, 0.0f, halfWidth, halfHeight);

    rc.commandList->SetPipelineState(m_psoBlur.get());

    ITexture* blurTextures[] = { fogTex, gbuffer2 };
    rc.commandList->PSSetTextures(0, 2, blurTextures);

    rc.commandList->Draw(3, 0);

    // お片付け
    ITexture* null2[2] = { nullptr, nullptr };
    rc.commandList->PSSetTextures(0, 2, null2);
    rc.commandList->PSSetSampler(2, nullptr);
    rc.commandList->PSSetSampler(1, nullptr);
}
