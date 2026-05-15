#include "TerrainRenderPass.h"
#include "Graphics.h"
#include "RHI/IResourceFactory.h"
#include "RHI/ICommandList.h"
#include "RHI/PipelineStateDesc.h"
#include "RenderContext/RenderContext.h"
#include "RenderContext/RenderQueue.h"
#include "RenderContext/RenderState.h"
#include "RenderGraph/FrameGraphBuilder.h"
#include "RenderGraph/FrameGraphResources.h"
#include <DirectXMath.h>

using namespace DirectX;

namespace {

struct TerrainCB {
    XMFLOAT4X4 viewProj;
    XMFLOAT4   chunkOffset;
    float      heightScale;
    float      pad[3];
};

} // namespace

TerrainRenderPass::~TerrainRenderPass() = default;

void TerrainRenderPass::Initialize()
{
    if (m_initialized) return;
    m_initialized = true;

    auto* factory = Graphics::Instance().GetResourceFactory();
    if (!factory) return;

    m_vs = factory->CreateShader(ShaderType::Vertex, "Data/Shader/TerrainVS.cso");
    m_ps = factory->CreateShader(ShaderType::Pixel,  "Data/Shader/TerrainPS.cso");
    if (!m_vs || !m_ps) return;

    static const InputLayoutElement kLayout[] = {
        { "POSITION", 0, TextureFormat::R32G32B32_FLOAT, 0, 0  },
        { "NORMAL",   0, TextureFormat::R32G32B32_FLOAT, 0, 12 },
        { "TEXCOORD", 0, TextureFormat::R32G32_FLOAT,    0, 24 },
    };
    m_inputLayout = factory->CreateInputLayout(InputLayoutDesc{ kLayout, 3 }, m_vs.get());
    m_cbTerrain   = factory->CreateBuffer(sizeof(TerrainCB), BufferType::Constant);

    auto* rs = Graphics::Instance().GetRenderState();
    if (!rs) return;

    PipelineStateDesc psoDesc{};
    psoDesc.vertexShader      = m_vs.get();
    psoDesc.pixelShader       = m_ps.get();
    psoDesc.inputLayout       = m_inputLayout.get();
    psoDesc.blendState        = rs->GetBlendState(BlendState::Opaque);
    psoDesc.rasterizerState   = rs->GetRasterizerState(RasterizerState::SolidCullBack);
    psoDesc.depthStencilState = rs->GetDepthStencilState(DepthState::TestAndWrite);
    psoDesc.primitiveTopology = PrimitiveTopology::TriangleList;
    psoDesc.numRenderTargets  = 1;
    psoDesc.rtvFormats[0]     = TextureFormat::R16G16B16A16_FLOAT;
    psoDesc.dsvFormat         = TextureFormat::D32_FLOAT;
    m_pso = factory->CreatePipelineState(psoDesc);
}

void TerrainRenderPass::Setup(FrameGraphBuilder& builder, const RenderContext& /*rc*/)
{
    m_hSceneColor = builder.GetHandle("SceneColor");
    m_hDepth      = builder.GetHandle("GBufferDepth");
    if (m_hSceneColor.IsValid()) {
        m_hSceneColor = builder.Write(m_hSceneColor);
        builder.RegisterHandle("SceneColor", m_hSceneColor);
    }
    if (m_hDepth.IsValid()) builder.Read(m_hDepth);
}

void TerrainRenderPass::Execute(
    FrameGraphResources& resources,
    const RenderQueue& queue,
    RenderContext& rc)
{
    if (queue.terrainChunks.empty()) return;
    if (!m_initialized) Initialize();
    if (!m_vs || !m_ps || !m_pso) return;

    ITexture* rtScene = resources.GetTexture(m_hSceneColor);
    ITexture* dsDepth = resources.GetTexture(m_hDepth);
    if (!rtScene || !dsDepth) return;

    auto* cmd = rc.commandList;
    auto* rs  = rc.renderState;
    if (!cmd) return;

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
        cmd->SetDepthStencilState(rs->GetDepthStencilState(DepthState::TestAndWrite));
        cmd->SetRasterizerState(rs->GetRasterizerState(RasterizerState::SolidCullBack));
        cmd->SetBlendState(rs->GetBlendState(BlendState::Opaque));
    }
    cmd->VSSetShader(m_vs.get());
    cmd->PSSetShader(m_ps.get());
    cmd->VSSetConstantBuffer(0, m_cbTerrain.get());

    XMMATRIX vp = XMMatrixTranspose(XMLoadFloat4x4(&rc.viewProjectionUnjittered));

    constexpr uint32_t kStride = sizeof(XMFLOAT3) + sizeof(XMFLOAT3) + sizeof(XMFLOAT2); // 32
    for (const TerrainChunkDrawCall& dc : queue.terrainChunks) {
        if (!dc.vertexBuffer || !dc.indexBuffer) continue;

        TerrainCB cb{};
        XMStoreFloat4x4(&cb.viewProj, vp);
        cb.chunkOffset = XMFLOAT4(dc.chunkWorldOffset.x, dc.chunkWorldOffset.y, dc.chunkWorldOffset.z, 0.0f);
        cb.heightScale = dc.heightScale;
        cmd->UpdateBuffer(m_cbTerrain.get(), &cb, sizeof(TerrainCB));

        cmd->SetVertexBuffer(0, dc.vertexBuffer, kStride);
        cmd->SetIndexBuffer(dc.indexBuffer, IndexFormat::Uint32);
        cmd->DrawIndexed(dc.indexCount, 0, 0);
    }

    cmd->SetRenderTarget(nullptr, nullptr);
}
