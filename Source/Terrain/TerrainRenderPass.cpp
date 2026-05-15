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
#include "RHI/DX12/DX12CommandList.h"
#include <DirectXMath.h>
#include <DirectXTex.h>

using namespace DirectX;

namespace {

struct TerrainCB {
    XMFLOAT4X4 viewProj;                  // current jittered VP
    XMFLOAT4X4 viewProjectionUnjittered;  // current unjittered VP (velocity)
    XMFLOAT4X4 prevViewProjection;        // previous-frame VP (velocity)
    XMFLOAT4   chunkOffset;
    float      heightScale;
    float      pad[3];
};

struct TerrainMaterialCB {
    XMFLOAT4 tileScales;
    XMFLOAT4 triplanarParams;  // x = strength
};

} // anonymous namespace

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
    m_cbTerrain   = factory->CreateBuffer(sizeof(TerrainCB),         BufferType::Constant);
    m_cbMaterial  = factory->CreateBuffer(sizeof(TerrainMaterialCB), BufferType::Constant);

    // Default 1x1 textures so missing slots do not abort the draw.
    auto Create1x1 = [&](uint8_t r, uint8_t g, uint8_t b, uint8_t a) -> std::unique_ptr<ITexture> {
        DirectX::ScratchImage img;
        if (FAILED(img.Initialize2D(DXGI_FORMAT_R8G8B8A8_UNORM, 1, 1, 1, 1))) return nullptr;
        const DirectX::Image* im = img.GetImage(0, 0, 0);
        if (!im || !im->pixels) return nullptr;
        im->pixels[0] = r; im->pixels[1] = g; im->pixels[2] = b; im->pixels[3] = a;
        return factory->CreateTextureFromMemory(img, img.GetMetadata());
    };
    m_defaultWhite      = Create1x1(255, 255, 255, 255);
    m_defaultFlatNormal = Create1x1(128, 128, 255, 255);    // (0, 0, 1) tangent space
    m_defaultMRA        = Create1x1(0,   217, 255, 255);    // M=0, R~0.85, AO=1.0

    auto* rs = Graphics::Instance().GetRenderState();
    if (!rs) return;

    PipelineStateDesc psoDesc{};
    psoDesc.vertexShader      = m_vs.get();
    psoDesc.pixelShader       = m_ps.get();
    psoDesc.inputLayout       = m_inputLayout.get();
    psoDesc.blendState        = rs->GetBlendState(BlendState::Opaque);
    psoDesc.rasterizerState   = rs->GetRasterizerState(RasterizerState::SolidCullNone);
    psoDesc.depthStencilState = rs->GetDepthStencilState(DepthState::TestAndWrite);
    psoDesc.primitiveTopology = PrimitiveTopology::TriangleList;
    psoDesc.numRenderTargets  = 4;
    psoDesc.rtvFormats[0]     = TextureFormat::R16G16B16A16_FLOAT;   // Albedo/Metallic
    psoDesc.rtvFormats[1]     = TextureFormat::R16G16B16A16_FLOAT;   // Normal/Roughness
    psoDesc.rtvFormats[2]     = TextureFormat::R32G32B32A32_FLOAT;   // WorldPos/Depth
    psoDesc.rtvFormats[3]     = TextureFormat::R32G32_FLOAT;         // Velocity
    psoDesc.dsvFormat         = TextureFormat::D32_FLOAT;
    m_pso = factory->CreatePipelineState(psoDesc);
}

void TerrainRenderPass::Setup(FrameGraphBuilder& builder, const RenderContext&)
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

void TerrainRenderPass::Execute(
    FrameGraphResources& resources,
    const RenderQueue& queue,
    RenderContext& rc)
{
    if (queue.terrainChunks.empty()) return;
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
    cmd->SetViewport(RhiViewport(
        0.0f, 0.0f,
        static_cast<float>(rt0->GetWidth()),
        static_cast<float>(rt0->GetHeight())));

    cmd->SetPrimitiveTopology(PrimitiveTopology::TriangleList);
    cmd->SetInputLayout(m_inputLayout.get());
    if (rs) {
        cmd->SetDepthStencilState(rs->GetDepthStencilState(DepthState::TestAndWrite));
        cmd->SetRasterizerState(rs->GetRasterizerState(RasterizerState::SolidCullNone));
        cmd->SetBlendState(rs->GetBlendState(BlendState::Opaque));
    }
    cmd->VSSetShader(m_vs.get());
    cmd->PSSetShader(m_ps.get());
    if (rs) {
        cmd->PSSetSampler(0, rs->GetSamplerState(SamplerState::LinearWrap));
    }

    DX12CommandList* dx12Cmd = nullptr;
    if (Graphics::Instance().GetAPI() == GraphicsAPI::DX12) {
        dx12Cmd = static_cast<DX12CommandList*>(cmd);
    }
    if (!dx12Cmd) {
        cmd->VSSetConstantBuffer(0, m_cbTerrain.get());
        cmd->PSSetConstantBuffer(0, m_cbTerrain.get());
    }

    // Terrain currently uses the unjittered VP (matches original behaviour); jitter
    // can be added later by routing rc.jitteredViewProjection when introduced.
    XMMATRIX vpUnjittered = XMMatrixTranspose(XMLoadFloat4x4(&rc.viewProjectionUnjittered));
    XMMATRIX prevVp       = XMMatrixTranspose(XMLoadFloat4x4(&rc.prevViewProjectionMatrix));
    XMMATRIX vp           = vpUnjittered;

    constexpr uint32_t kStride = sizeof(XMFLOAT3) + sizeof(XMFLOAT3) + sizeof(XMFLOAT2);
    ITexture* white      = m_defaultWhite.get();
    ITexture* flatNormal = m_defaultFlatNormal.get();
    ITexture* defaultMRA = m_defaultMRA.get();

    for (const TerrainChunkDrawCall& dc : queue.terrainChunks) {
        if (!dc.vertexBuffer || !dc.indexBuffer) continue;

        TerrainCB cb{};
        XMStoreFloat4x4(&cb.viewProj,                  vp);
        XMStoreFloat4x4(&cb.viewProjectionUnjittered,  vpUnjittered);
        XMStoreFloat4x4(&cb.prevViewProjection,        prevVp);
        cb.chunkOffset = XMFLOAT4(dc.chunkWorldOffset.x, dc.chunkWorldOffset.y, dc.chunkWorldOffset.z, 0.0f);
        cb.heightScale = dc.heightScale;

        if (dx12Cmd) {
            dx12Cmd->VSSetDynamicConstantBuffer(0, &cb, sizeof(TerrainCB));
            dx12Cmd->PSSetDynamicConstantBuffer(0, &cb, sizeof(TerrainCB));
        } else {
            cmd->UpdateBuffer(m_cbTerrain.get(), &cb, sizeof(TerrainCB));
        }

        TerrainMaterialCB matCB{};
        matCB.tileScales = XMFLOAT4(
            dc.layerTileScales[0], dc.layerTileScales[1], dc.layerTileScales[2], 0.0f);
        matCB.triplanarParams = XMFLOAT4(0.6f, 0.0f, 0.0f, 0.0f);  // triplanar strength
        if (dx12Cmd) {
            dx12Cmd->PSSetDynamicConstantBuffer(1, &matCB, sizeof(TerrainMaterialCB));
        } else {
            cmd->UpdateBuffer(m_cbMaterial.get(), &matCB, sizeof(TerrainMaterialCB));
            cmd->PSSetConstantBuffer(1, m_cbMaterial.get());
        }

        // Bind 10 texture SRVs: t0=splat, t1-t3=albedo, t4-t6=normal, t7-t9=MRA.
        ITexture* splat = dc.splatTexture ? dc.splatTexture : white;
        ITexture* a0    = dc.albedoTextures[0] ? dc.albedoTextures[0] : white;
        ITexture* a1    = dc.albedoTextures[1] ? dc.albedoTextures[1] : white;
        ITexture* a2    = dc.albedoTextures[2] ? dc.albedoTextures[2] : white;
        ITexture* n0    = dc.normalTextures[0] ? dc.normalTextures[0] : flatNormal;
        ITexture* n1    = dc.normalTextures[1] ? dc.normalTextures[1] : flatNormal;
        ITexture* n2    = dc.normalTextures[2] ? dc.normalTextures[2] : flatNormal;
        ITexture* m0    = dc.mraTextures[0]    ? dc.mraTextures[0]    : defaultMRA;
        ITexture* m1    = dc.mraTextures[1]    ? dc.mraTextures[1]    : defaultMRA;
        ITexture* m2    = dc.mraTextures[2]    ? dc.mraTextures[2]    : defaultMRA;

        if (dx12Cmd) {
            DX12CommandList::PixelTextureBinding bindings[] = {
                { 0, splat, DX12CommandList::NullSrvKind::Texture2D },
                { 1, a0,    DX12CommandList::NullSrvKind::Texture2D },
                { 2, a1,    DX12CommandList::NullSrvKind::Texture2D },
                { 3, a2,    DX12CommandList::NullSrvKind::Texture2D },
                { 4, n0,    DX12CommandList::NullSrvKind::Texture2D },
                { 5, n1,    DX12CommandList::NullSrvKind::Texture2D },
                { 6, n2,    DX12CommandList::NullSrvKind::Texture2D },
                { 7, m0,    DX12CommandList::NullSrvKind::Texture2D },
                { 8, m1,    DX12CommandList::NullSrvKind::Texture2D },
                { 9, m2,    DX12CommandList::NullSrvKind::Texture2D },
            };
            dx12Cmd->BindPixelTextureTable(bindings, _countof(bindings));
        } else {
            ITexture* slots[10] = { splat, a0, a1, a2, n0, n1, n2, m0, m1, m2 };
            cmd->PSSetTextures(0, 10, slots);
        }

        cmd->SetVertexBuffer(0, dc.vertexBuffer, kStride);
        cmd->SetIndexBuffer(dc.indexBuffer, IndexFormat::Uint32);
        cmd->DrawIndexed(dc.indexCount, 0, 0);
    }

    cmd->SetRenderTarget(nullptr, nullptr);
}
