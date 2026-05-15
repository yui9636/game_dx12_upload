#include "WaterRenderPass.h"
#include "Graphics.h"
#include "RHI/IResourceFactory.h"
#include "RHI/ICommandList.h"
#include "RHI/PipelineStateDesc.h"
#include "RenderContext/RenderContext.h"
#include "RenderContext/RenderQueue.h"
#include "RenderContext/RenderState.h"
#include "RenderGraph/FrameGraphBuilder.h"
#include "RenderGraph/FrameGraphResources.h"
#include "RHI/DX12/DX12CommandList.h"
#include <DirectXMath.h>

using namespace DirectX;

namespace
{
    struct WaterCB {
        XMFLOAT4X4 viewProj;
        XMFLOAT4 worldOffsetSeaLevel;
        XMFLOAT4 shallowColor;
        XMFLOAT4 deepColor;
        XMFLOAT4 params;
        XMFLOAT4 cameraPosition;
        XMFLOAT4 screenParams;
        XMFLOAT4 waterFeatureFlags;
    };
}

WaterRenderPass::~WaterRenderPass() = default;

void WaterRenderPass::Initialize()
{
    if (m_initialized) return;
    m_initialized = true;

    auto* factory = Graphics::Instance().GetResourceFactory();
    if (!factory) return;

    m_vs = factory->CreateShader(ShaderType::Vertex, "Data/Shader/WaterVS.cso");
    m_ps = factory->CreateShader(ShaderType::Pixel,  "Data/Shader/WaterPS.cso");
    if (!m_vs || !m_ps) return;

    static const InputLayoutElement kLayout[] = {
        { "POSITION", 0, TextureFormat::R32G32B32_FLOAT, 0, 0  },
        { "TEXCOORD", 0, TextureFormat::R32G32_FLOAT,    0, 12 },
        { "TEXCOORD", 1, TextureFormat::R32G32_FLOAT,    0, 20 },
    };
    m_inputLayout = factory->CreateInputLayout(InputLayoutDesc{ kLayout, 3 }, m_vs.get());
    m_cbWater = factory->CreateBuffer(sizeof(WaterCB), BufferType::Constant);

    auto* rs = Graphics::Instance().GetRenderState();
    if (!rs) return;

    PipelineStateDesc psoDesc{};
    psoDesc.vertexShader      = m_vs.get();
    psoDesc.pixelShader       = m_ps.get();
    psoDesc.inputLayout       = m_inputLayout.get();
    psoDesc.blendState        = rs->GetBlendState(BlendState::Alpha);
    psoDesc.rasterizerState   = rs->GetRasterizerState(RasterizerState::SolidCullNone);
    psoDesc.depthStencilState = rs->GetDepthStencilState(DepthState::TestOnly);
    psoDesc.primitiveTopology = PrimitiveTopology::TriangleList;
    psoDesc.numRenderTargets  = 1;
    psoDesc.rtvFormats[0]     = TextureFormat::R16G16B16A16_FLOAT;
    psoDesc.dsvFormat         = TextureFormat::D32_FLOAT;
    m_pso = factory->CreatePipelineState(psoDesc);
}

void WaterRenderPass::Setup(FrameGraphBuilder& builder, const RenderContext&)
{
    m_hSceneColor = builder.GetHandle("SceneColor");
    m_hDepth      = builder.GetHandle("GBufferDepth");
    m_hGBuffer2   = builder.GetHandle("GBuffer2");
    m_hPrevScene  = builder.GetHandle("PrevScene");

    if (m_hSceneColor.IsValid()) {
        m_hSceneColor = builder.Write(m_hSceneColor);
        builder.RegisterHandle("SceneColor", m_hSceneColor);
    }
    if (m_hDepth.IsValid()) {
        builder.Read(m_hDepth);
    }
    if (m_hGBuffer2.IsValid()) {
        builder.Read(m_hGBuffer2);
    }
    if (m_hPrevScene.IsValid()) {
        builder.Read(m_hPrevScene);
    }
}

void WaterRenderPass::Execute(FrameGraphResources& resources, const RenderQueue& queue, RenderContext& rc)
{
    if (queue.terrainWater.empty()) return;
    if (!m_initialized) Initialize();
    if (!m_vs || !m_ps || !m_pso) return;

    ITexture* rtScene = resources.GetTexture(m_hSceneColor);
    ITexture* dsDepth = resources.GetTexture(m_hDepth);
    if (!rtScene || !dsDepth || !rc.commandList) return;

    auto* cmd = rc.commandList;
    auto* rs = rc.renderState;

    cmd->SetPipelineState(m_pso.get());
    cmd->TransitionBarrier(rtScene, ResourceState::RenderTarget);
    cmd->TransitionBarrier(dsDepth, ResourceState::DepthWrite);
    cmd->SetRenderTarget(rtScene, dsDepth);
    cmd->SetViewport(RhiViewport(
        0.0f, 0.0f,
        static_cast<float>(rtScene->GetWidth()),
        static_cast<float>(rtScene->GetHeight())));
    cmd->SetPrimitiveTopology(PrimitiveTopology::TriangleList);
    cmd->SetInputLayout(m_inputLayout.get());
    if (rs) {
        cmd->SetDepthStencilState(rs->GetDepthStencilState(DepthState::TestOnly));
        cmd->SetRasterizerState(rs->GetRasterizerState(RasterizerState::SolidCullNone));
        cmd->SetBlendState(rs->GetBlendState(BlendState::Alpha));
    }
    cmd->VSSetShader(m_vs.get());
    cmd->PSSetShader(m_ps.get());

    DX12CommandList* dx12Cmd = nullptr;
    if (Graphics::Instance().GetAPI() == GraphicsAPI::DX12) {
        dx12Cmd = static_cast<DX12CommandList*>(cmd);
    }
    if (!dx12Cmd) {
        cmd->VSSetConstantBuffer(0, m_cbWater.get());
        cmd->PSSetConstantBuffer(0, m_cbWater.get());
    }

    // Bind GBuffer2 (worldPos/depth) and PrevScene as PS shader resources.
    ITexture* gbuffer2 = m_hGBuffer2.IsValid() ? resources.GetTexture(m_hGBuffer2) : nullptr;
    ITexture* prevScene = m_hPrevScene.IsValid() ? resources.GetTexture(m_hPrevScene) : nullptr;
    if (gbuffer2) cmd->TransitionBarrier(gbuffer2, ResourceState::ShaderResource);
    if (prevScene) cmd->TransitionBarrier(prevScene, ResourceState::ShaderResource);

    if (dx12Cmd) {
        DX12CommandList::PixelTextureBinding waterBindings[] = {
            { 0, gbuffer2,  DX12CommandList::NullSrvKind::Texture2D },
            { 1, prevScene, DX12CommandList::NullSrvKind::Texture2D },
        };
        dx12Cmd->BindPixelTextureTable(waterBindings, _countof(waterBindings));
    } else {
        ITexture* waterSRVs[] = { gbuffer2, prevScene };
        cmd->PSSetTextures(0, 2, waterSRVs);
    }
    if (rs) {
        cmd->PSSetSampler(2, rs->GetSamplerState(SamplerState::PointClamp));
        cmd->PSSetSampler(3, rs->GetSamplerState(SamplerState::LinearClamp));
    }

    const XMMATRIX vp = XMMatrixTranspose(XMLoadFloat4x4(&rc.viewProjectionUnjittered));
    const float renderW = static_cast<float>(rtScene->GetWidth());
    const float renderH = static_cast<float>(rtScene->GetHeight());
    const float hasRefraction = (prevScene != nullptr) ? 1.0f : 0.0f;
    constexpr uint32_t kStride = sizeof(XMFLOAT3) + sizeof(XMFLOAT2) + sizeof(XMFLOAT2);
    for (const TerrainWaterDrawCall& water : queue.terrainWater) {
        if (!water.vertexBuffer || !water.indexBuffer || water.indexCount == 0) continue;

        WaterCB cb{};
        XMStoreFloat4x4(&cb.viewProj, vp);
        cb.worldOffsetSeaLevel = {
            water.worldOffset.x,
            water.worldOffset.y,
            water.worldOffset.z,
            water.seaLevel
        };
        cb.shallowColor = water.shallowColor;
        cb.deepColor = water.deepColor;
        cb.params = { rc.time, water.waveSpeed, water.waveScale, water.depthFade };
        cb.cameraPosition = { rc.cameraPosition.x, rc.cameraPosition.y, rc.cameraPosition.z, 0.0f };
        cb.screenParams = { renderW, renderH, 1.0f / renderW, 1.0f / renderH };
        cb.waterFeatureFlags = { hasRefraction, 0.0f, 0.0f, 0.0f };

        if (dx12Cmd) {
            dx12Cmd->VSSetDynamicConstantBuffer(0, &cb, sizeof(WaterCB));
            dx12Cmd->PSSetDynamicConstantBuffer(0, &cb, sizeof(WaterCB));
        } else {
            cmd->UpdateBuffer(m_cbWater.get(), &cb, sizeof(WaterCB));
        }

        cmd->SetVertexBuffer(0, water.vertexBuffer, kStride);
        cmd->SetIndexBuffer(water.indexBuffer, IndexFormat::Uint32);
        cmd->DrawIndexed(water.indexCount, 0, 0);
    }

    cmd->SetRenderTarget(nullptr, nullptr);
}
