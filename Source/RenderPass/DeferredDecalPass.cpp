#include "DeferredDecalPass.h"

#include "Render/Graphics.h"
#include "RHI/IShader.h"
#include "RHI/ITexture.h"
#include "RHI/IBuffer.h"
#include "RHI/IResourceFactory.h"
#include "RHI/ICommandList.h"
#include "RHI/PipelineStateDesc.h"
#include "RHI/IPipelineState.h"
#include "RHI/DX12/DX12CommandList.h"
#include "RenderContext/RenderState.h"
#include "RenderGraph/FrameGraphBuilder.h"
#include "RenderGraph/FrameGraphResources.h"
#include "Console/Logger.h"

#include <DirectXMath.h>

namespace {
    // DecalVS.hlsl / DecalPS.hlsl の CbDecal と同じ 176 bytes layout に合わせる。
    struct CbDecal {
        DirectX::XMFLOAT4X4 decalWorldViewProj;
        DirectX::XMFLOAT4X4 worldToDecal;
        DirectX::XMFLOAT4   tintOpacity;
        DirectX::XMFLOAT4   params;       // x=angleFade, y=invWidth, z=invHeight, w=0
        DirectX::XMFLOAT4   decalAxisWS;  // xyz=projection axis
    };
}

DeferredDecalPass::~DeferredDecalPass() = default;

DeferredDecalPass::DeferredDecalPass(IResourceFactory* factory)
{
    m_vs = factory->CreateShader(ShaderType::Vertex, "Data/Shader/DecalVS.cso");
    m_ps = factory->CreateShader(ShaderType::Pixel,  "Data/Shader/DecalPS.cso");
    m_cb = factory->CreateBuffer(sizeof(CbDecal), BufferType::Constant);

    auto* rs = Graphics::Instance().GetRenderState();
    PipelineStateDesc desc{};
    desc.vertexShader      = m_vs.get();
    desc.pixelShader       = m_ps.get();
    desc.inputLayout       = nullptr; // cube は SV_VertexID から shader 側で生成する。
    desc.depthStencilState = rs->GetDepthStencilState(DepthState::NoTestNoWrite);
    desc.rasterizerState   = rs->GetRasterizerState(RasterizerState::SolidCullNone);
    desc.blendState        = rs->GetBlendState(BlendState::Transparency);
    desc.primitiveTopology = PrimitiveTopology::TriangleList;
    desc.numRenderTargets  = 1;
    desc.rtvFormats[0]     = TextureFormat::R16G16B16A16_FLOAT; // GBuffer0 と同じ形式。
    desc.dsvFormat         = TextureFormat::Unknown;
    m_pso = factory->CreatePipelineState(desc);
}

void DeferredDecalPass::Setup(FrameGraphBuilder& builder, const RenderContext& rc)
{
    // Albedo だけを書き換え、normal/world position は投影判定のために読む。
    m_hGBuffer0 = builder.GetHandle("GBuffer0");
    m_hGBuffer1 = builder.GetHandle("GBuffer1");
    m_hGBuffer2 = builder.GetHandle("GBuffer2");

    if (m_hGBuffer1.IsValid()) builder.Read(m_hGBuffer1);
    if (m_hGBuffer2.IsValid()) builder.Read(m_hGBuffer2);

    if (m_hGBuffer0.IsValid()) {
        m_hGBuffer0 = builder.Write(m_hGBuffer0);
        builder.RegisterHandle("GBuffer0", m_hGBuffer0);
    }
}

void DeferredDecalPass::Execute(FrameGraphResources& resources, const RenderQueue& queue, RenderContext& rc)
{
    static int s_frame = 0;
    const bool logThisFrame = (s_frame++ % 120) == 0;
    if (rc.decals.empty()) {
        return;
    }

    ITexture* gbuffer0 = resources.GetTexture(m_hGBuffer0);
    ITexture* gbuffer1 = resources.GetTexture(m_hGBuffer1);
    ITexture* gbuffer2 = resources.GetTexture(m_hGBuffer2);
    if (!gbuffer0 || !gbuffer1 || !gbuffer2) {
        if (logThisFrame) {
            LOG_ERROR("[DeferredDecalPass] missing GBuffer: gb0=%p gb1=%p gb2=%p", gbuffer0, gbuffer1, gbuffer2);
        }
        return;
    }
    if (logThisFrame) {
        LOG_INFO("[DeferredDecalPass] executing: %zu decals, rt=%ux%u",
            rc.decals.size(), gbuffer0->GetWidth(), gbuffer0->GetHeight());
    }

    using namespace DirectX;
    const XMMATRIX view = XMLoadFloat4x4(&rc.viewMatrix);
    const XMMATRIX proj = XMLoadFloat4x4(&rc.projectionMatrix);
    const XMMATRIX viewProj = view * proj;

    const float width  = static_cast<float>(gbuffer0->GetWidth());
    const float height = static_cast<float>(gbuffer0->GetHeight());

    rc.commandList->SetRenderTarget(gbuffer0, nullptr);
    rc.commandList->SetViewport(RhiViewport(0.0f, 0.0f, width, height));
    rc.commandList->SetPipelineState(m_pso.get());
    rc.commandList->SetPrimitiveTopology(PrimitiveTopology::TriangleList);

    auto* linearSampler = rc.renderState->GetSamplerState(SamplerState::LinearClamp);
    rc.commandList->PSSetSampler(0, linearSampler);

    const bool isDX12 = Graphics::Instance().GetAPI() == GraphicsAPI::DX12;

    // decal ごとに box volume を描き、pixel shader 側で GBuffer 座標へ投影する。
    for (const DecalInstance& decal : rc.decals) {
        if (!decal.texture) {
            continue;
        }

        CbDecal cb{};
        const XMMATRIX boxWorld = XMLoadFloat4x4(&decal.worldMatrix);
        XMStoreFloat4x4(&cb.decalWorldViewProj, boxWorld * viewProj);
        cb.worldToDecal = decal.invWorldMatrix;
        cb.tintOpacity  = decal.tintOpacity;
        cb.params       = { decal.angleFade,
                            width  > 0.0f ? 1.0f / width  : 0.0f,
                            height > 0.0f ? 1.0f / height : 0.0f,
                            0.0f };
        cb.decalAxisWS  = { decal.projectionAxis.x, decal.projectionAxis.y, decal.projectionAxis.z, 0.0f };

        if (isDX12) {
            auto* dx12 = static_cast<DX12CommandList*>(rc.commandList);
            dx12->VSSetDynamicConstantBuffer(1, &cb, sizeof(cb));
            dx12->PSSetDynamicConstantBuffer(1, &cb, sizeof(cb));
        } else {
            rc.commandList->UpdateBuffer(m_cb.get(), &cb, sizeof(cb));
            rc.commandList->VSSetConstantBuffer(1, m_cb.get());
            rc.commandList->PSSetConstantBuffer(1, m_cb.get());
        }

        ITexture* srvs[] = { gbuffer2, gbuffer1, decal.texture };
        rc.commandList->PSSetTextures(0, 3, srvs);
        rc.commandList->Draw(36, 0);
    }

    ITexture* nulls[] = { nullptr, nullptr, nullptr };
    rc.commandList->PSSetTextures(0, 3, nulls);
}
