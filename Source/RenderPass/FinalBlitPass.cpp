#include "FinalBlitPass.h"
#include "Graphics.h"
#include "RHI/IResourceFactory.h"
#include "RHI/ICommandList.h"
#include "RHI/IShader.h"
#include "RHI/IPipelineState.h"
#include "RHI/PipelineStateDesc.h"
#include "RHI/ITexture.h"
#include "RenderGraph/FrameGraphBuilder.h"
#include "RenderGraph/FrameGraphResources.h"
#include "Console/Logger.h"

FinalBlitPass::FinalBlitPass(IResourceFactory* factory)
{
    m_vs = factory->CreateShader(ShaderType::Vertex, "Data/Shader/FullScreenQuadVS.cso");
    m_ps = factory->CreateShader(ShaderType::Pixel, "Data/Shader/FinalBlitPS.cso");

    auto* rs = Graphics::Instance().GetRenderState();
    PipelineStateDesc desc{};
    desc.vertexShader = m_vs.get();
    desc.pixelShader = m_ps.get();
    desc.inputLayout = nullptr;
    desc.depthStencilState = rs->GetDepthStencilState(DepthState::NoTestNoWrite);
    desc.rasterizerState = rs->GetRasterizerState(RasterizerState::SolidCullNone);
    desc.blendState = rs->GetBlendState(BlendState::Opaque);
    desc.primitiveTopology = PrimitiveTopology::TriangleStrip;
    desc.numRenderTargets = 1;
    if (FrameBuffer* display = Graphics::Instance().GetFrameBuffer(FrameBufferId::Display)) {
        if (ITexture* displayColor = display->GetColorTexture(0)) {
            desc.rtvFormats[0] = displayColor->GetFormat();
        }
    }
    desc.dsvFormat = TextureFormat::Unknown;
    m_pso = factory->CreatePipelineState(desc);
}

void FinalBlitPass::Setup(FrameGraphBuilder& builder)
{
    m_hSceneColor = builder.GetHandle("SceneColor");
    m_hDisplayColor = builder.GetHandle("DisplayColor");

    if (m_hSceneColor.IsValid()) {
        builder.Read(m_hSceneColor);
    }
    if (m_hDisplayColor.IsValid()) {
        m_hDisplayColor = builder.Write(m_hDisplayColor);
        builder.RegisterHandle("DisplayColor", m_hDisplayColor);
    }
}

void FinalBlitPass::Execute(FrameGraphResources& resources, const RenderQueue& queue, RenderContext& rc)
{
}
