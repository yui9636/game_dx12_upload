#include "GrassRenderPass.h"
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
#include <cmath>

using namespace DirectX;

namespace {

struct GrassCB {
    XMFLOAT4X4 viewProj;
    XMFLOAT4X4 viewProjectionUnjittered;
    XMFLOAT4X4 prevViewProjection;
    XMFLOAT4   windDirSpeed;       // xyz = dir, w = speed
    XMFLOAT4   windStrengthTime;   // x = strength, y = time
    XMFLOAT4   colorBottom;
    XMFLOAT4   colorTop;
    XMFLOAT4   cameraPosition;
};

// 32 bytes per instance, layout matches GrassInstanceData.
constexpr uint32_t kInstanceStride = 32u;
// Per-vertex (slot 0) stride: float3 position + float2 uv = 20 bytes.
constexpr uint32_t kVertexStride = sizeof(XMFLOAT3) + sizeof(XMFLOAT2);

} // anonymous namespace

GrassRenderPass::~GrassRenderPass() = default;

void GrassRenderPass::Initialize()
{
    if (m_initialized) return;
    m_initialized = true;

    auto* factory = Graphics::Instance().GetResourceFactory();
    if (!factory) return;

    m_vs = factory->CreateShader(ShaderType::Vertex, "Data/Shader/GrassVS.cso");
    m_ps = factory->CreateShader(ShaderType::Pixel,  "Data/Shader/GrassPS.cso");
    if (!m_vs || !m_ps) return;

    // Per-vertex (slot 0): POSITION (12), TEXCOORD0 (8)
    // Per-instance (slot 1): TEXCOORD1 (worldPos, 12), TEXCOORD2 (scale, 4),
    //                        TEXCOORD3 (tint, 12), TEXCOORD4 (rotY, 4)
    static const InputLayoutElement kLayout[] = {
        { "POSITION", 0, TextureFormat::R32G32B32_FLOAT, 0,  0,  false, 0 },
        { "TEXCOORD", 0, TextureFormat::R32G32_FLOAT,    0, 12,  false, 0 },
        { "TEXCOORD", 1, TextureFormat::R32G32B32_FLOAT, 1,  0,  true,  1 },
        { "TEXCOORD", 2, TextureFormat::R32_FLOAT,       1, 12,  true,  1 },
        { "TEXCOORD", 3, TextureFormat::R32G32B32_FLOAT, 1, 16,  true,  1 },
        { "TEXCOORD", 4, TextureFormat::R32_FLOAT,       1, 28,  true,  1 },
    };
    m_inputLayout = factory->CreateInputLayout(InputLayoutDesc{ kLayout, 6 }, m_vs.get());
    m_cb = factory->CreateBuffer(sizeof(GrassCB), BufferType::Constant);

    auto* rs = Graphics::Instance().GetRenderState();
    if (!rs) return;

    PipelineStateDesc psoDesc{};
    psoDesc.vertexShader      = m_vs.get();
    psoDesc.pixelShader       = m_ps.get();
    psoDesc.inputLayout       = m_inputLayout.get();
    psoDesc.blendState        = rs->GetBlendState(BlendState::Opaque);     // alpha-tested = opaque blend
    psoDesc.rasterizerState   = rs->GetRasterizerState(RasterizerState::SolidCullNone);
    psoDesc.depthStencilState = rs->GetDepthStencilState(DepthState::TestAndWrite);
    psoDesc.primitiveTopology = PrimitiveTopology::TriangleList;
    psoDesc.numRenderTargets  = 4;
    psoDesc.rtvFormats[0]     = TextureFormat::R16G16B16A16_FLOAT;
    psoDesc.rtvFormats[1]     = TextureFormat::R16G16B16A16_FLOAT;
    psoDesc.rtvFormats[2]     = TextureFormat::R32G32B32A32_FLOAT;
    psoDesc.rtvFormats[3]     = TextureFormat::R32G32_FLOAT;
    psoDesc.dsvFormat         = TextureFormat::D32_FLOAT;
    m_pso = factory->CreatePipelineState(psoDesc);
}

void GrassRenderPass::Setup(FrameGraphBuilder& builder, const RenderContext&)
{
    m_hGBuffer0 = builder.GetHandle("GBuffer0");
    m_hGBuffer1 = builder.GetHandle("GBuffer1");
    m_hGBuffer2 = builder.GetHandle("GBuffer2");
    m_hGBuffer3 = builder.GetHandle("GBuffer3");
    m_hDepth    = builder.GetHandle("GBufferDepth");

    if (m_hGBuffer0.IsValid()) {
        m_hGBuffer0 = builder.Write(m_hGBuffer0);
        builder.RegisterHandle("GBuffer0", m_hGBuffer0);
    }
    if (m_hGBuffer1.IsValid()) {
        m_hGBuffer1 = builder.Write(m_hGBuffer1);
        builder.RegisterHandle("GBuffer1", m_hGBuffer1);
    }
    if (m_hGBuffer2.IsValid()) {
        m_hGBuffer2 = builder.Write(m_hGBuffer2);
        builder.RegisterHandle("GBuffer2", m_hGBuffer2);
    }
    if (m_hGBuffer3.IsValid()) {
        m_hGBuffer3 = builder.Write(m_hGBuffer3);
        builder.RegisterHandle("GBuffer3", m_hGBuffer3);
    }
    if (m_hDepth.IsValid()) {
        m_hDepth = builder.Write(m_hDepth);
        builder.RegisterHandle("GBufferDepth", m_hDepth);
    }
}

void GrassRenderPass::Execute(FrameGraphResources& resources, const RenderQueue& queue, RenderContext& rc)
{
    if (queue.grassDraws.empty()) return;
    if (!m_initialized) Initialize();
    if (!m_vs || !m_ps || !m_pso) return;

    ITexture* rt0 = resources.GetTexture(m_hGBuffer0);
    ITexture* rt1 = resources.GetTexture(m_hGBuffer1);
    ITexture* rt2 = resources.GetTexture(m_hGBuffer2);
    ITexture* rt3 = resources.GetTexture(m_hGBuffer3);
    ITexture* dsDepth = resources.GetTexture(m_hDepth);
    if (!rt0 || !rt1 || !rt2 || !rt3 || !dsDepth) return;

    auto* cmd = rc.commandList;
    auto* rs  = rc.renderState;
    if (!cmd) return;

    cmd->SetPipelineState(m_pso.get());
    cmd->TransitionBarrier(rt0, ResourceState::RenderTarget);
    cmd->TransitionBarrier(rt1, ResourceState::RenderTarget);
    cmd->TransitionBarrier(rt2, ResourceState::RenderTarget);
    cmd->TransitionBarrier(rt3, ResourceState::RenderTarget);
    cmd->TransitionBarrier(dsDepth, ResourceState::DepthWrite);
    ITexture* rts[4] = { rt0, rt1, rt2, rt3 };
    cmd->SetRenderTargets(4, rts, dsDepth);
    cmd->SetViewport(RhiViewport(0.0f, 0.0f,
        static_cast<float>(rt0->GetWidth()), static_cast<float>(rt0->GetHeight())));
    cmd->SetPrimitiveTopology(PrimitiveTopology::TriangleList);
    cmd->SetInputLayout(m_inputLayout.get());
    if (rs) {
        cmd->SetDepthStencilState(rs->GetDepthStencilState(DepthState::TestAndWrite));
        cmd->SetRasterizerState(rs->GetRasterizerState(RasterizerState::SolidCullNone));
        cmd->SetBlendState(rs->GetBlendState(BlendState::Opaque));
    }
    cmd->VSSetShader(m_vs.get());
    cmd->PSSetShader(m_ps.get());

    DX12CommandList* dx12Cmd = nullptr;
    if (Graphics::Instance().GetAPI() == GraphicsAPI::DX12) {
        dx12Cmd = static_cast<DX12CommandList*>(cmd);
    }
    if (!dx12Cmd) {
        cmd->VSSetConstantBuffer(0, m_cb.get());
        cmd->PSSetConstantBuffer(0, m_cb.get());
    }

    XMMATRIX vpUnjittered = XMMatrixTranspose(XMLoadFloat4x4(&rc.viewProjectionUnjittered));
    XMMATRIX prevVp       = XMMatrixTranspose(XMLoadFloat4x4(&rc.prevViewProjectionMatrix));
    XMMATRIX vp           = vpUnjittered;

    for (const GrassDrawCall& dc : queue.grassDraws) {
        if (!dc.meshVertexBuffer || !dc.meshIndexBuffer || !dc.instanceBuffer) continue;
        if (dc.instanceCount == 0 || dc.meshIndexCount == 0) continue;

        // Distance cull (entity bounds center vs camera)
        const float dx = dc.boundsCenter.x - rc.cameraPosition.x;
        const float dz = dc.boundsCenter.z - rc.cameraPosition.z;
        const float horizDist = std::sqrt(dx * dx + dz * dz)
            - std::max({ dc.boundsExtents.x, dc.boundsExtents.z });
        if (horizDist > dc.drawDistance) continue;

        GrassCB cb{};
        XMStoreFloat4x4(&cb.viewProj,                  vp);
        XMStoreFloat4x4(&cb.viewProjectionUnjittered,  vpUnjittered);
        XMStoreFloat4x4(&cb.prevViewProjection,        prevVp);
        cb.windDirSpeed      = { dc.windDirection.x, dc.windDirection.y, dc.windDirection.z, dc.windSpeed };
        cb.windStrengthTime  = { dc.windStrength, rc.time, 0.0f, 0.0f };
        cb.colorBottom       = { dc.colorBottom.x, dc.colorBottom.y, dc.colorBottom.z, 0.0f };
        cb.colorTop          = { dc.colorTop.x,    dc.colorTop.y,    dc.colorTop.z,    0.0f };
        cb.cameraPosition    = { rc.cameraPosition.x, rc.cameraPosition.y, rc.cameraPosition.z, 0.0f };

        if (dx12Cmd) {
            dx12Cmd->VSSetDynamicConstantBuffer(0, &cb, sizeof(GrassCB));
            dx12Cmd->PSSetDynamicConstantBuffer(0, &cb, sizeof(GrassCB));
        } else {
            cmd->UpdateBuffer(m_cb.get(), &cb, sizeof(GrassCB));
        }

        cmd->SetVertexBuffer(0, dc.meshVertexBuffer, kVertexStride);
        cmd->SetVertexBuffer(1, dc.instanceBuffer,   kInstanceStride);
        cmd->SetIndexBuffer(dc.meshIndexBuffer, IndexFormat::Uint16);
        cmd->DrawIndexedInstanced(dc.meshIndexCount, dc.instanceCount, 0, 0, 0);
    }

    cmd->SetRenderTarget(nullptr, nullptr);
}
