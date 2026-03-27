#include "SkyboxPass.h"
#include "Graphics.h"
#include "SkyBox.h"
#include "RenderGraph/FrameGraphBuilder.h"
#include "RenderGraph/FrameGraphResources.h"
#include "RHI/ICommandList.h"

void SkyboxPass::Setup(FrameGraphBuilder& builder, const RenderContext& rc)
{
    m_hSceneColor = builder.GetHandle("SceneColor");
    m_hDepth = builder.GetHandle("GBufferDepth");

    if (m_hSceneColor.IsValid()) {
        m_hSceneColor = builder.Write(m_hSceneColor);
        builder.RegisterHandle("SceneColor", m_hSceneColor);
    }
    if (m_hDepth.IsValid()) {
        builder.Read(m_hDepth);
    }
}

void SkyboxPass::Execute(FrameGraphResources& resources, const RenderQueue& queue, RenderContext& rc) {
    if (rc.environment.skyboxPath.empty()) return;

    ITexture* rtScene = resources.GetTexture(m_hSceneColor);
    ITexture* dsReal = resources.GetTexture(m_hDepth);

    if (!rtScene || !dsReal) return;

    IResourceFactory* factory = Graphics::Instance().GetResourceFactory();
    Skybox* skybox = Skybox::Get(factory, rc.environment.skyboxPath);

    if (skybox) {
        rc.commandList->TransitionBarrier(rtScene, ResourceState::RenderTarget);
        rc.commandList->TransitionBarrier(dsReal, ResourceState::DepthWrite);
        rc.commandList->SetRenderTarget(rtScene, dsReal);

        rc.mainRenderTarget = rtScene;
        rc.mainDepthStencil = dsReal;

        rc.mainViewport = RhiViewport(0.0f, 0.0f, (float)rtScene->GetWidth(), (float)rtScene->GetHeight());
        rc.commandList->SetViewport(rc.mainViewport);

        DirectX::XMMATRIX V = DirectX::XMLoadFloat4x4(&rc.viewMatrix);
        DirectX::XMMATRIX P = DirectX::XMLoadFloat4x4(&rc.projectionMatrix);

        V.r[3] = DirectX::XMVectorSet(0, 0, 0, 1);

        DirectX::XMFLOAT4X4 vp;
        DirectX::XMStoreFloat4x4(&vp, V * P);

        skybox->Draw(rc, vp);

        rc.commandList->SetRenderTarget(nullptr, nullptr);
    }
}
