#include "SkyboxPass.h"
#include "Graphics.h"
#include "SkyBox.h"
#include "RenderGraph/FrameGraphBuilder.h"
#include "RenderGraph/FrameGraphResources.h"
#include "RHI/ICommandList.h"
#include "Console/Logger.h"

void SkyboxPass::Setup(FrameGraphBuilder& builder)
{
    // 1. �������ݐ�i���C�e�B���O�ς݂̐F�j�� �ǂݍ��݌��i�[�x�j���擾
    m_hSceneColor = builder.GetHandle("SceneColor");
    m_hDepth = builder.GetHandle("GBufferDepth");

    // 2. �g�p�錾
    if (m_hSceneColor.IsValid()) {
        builder.Read(m_hSceneColor);
    }
    if (m_hDepth.IsValid()) {
        builder.Read(m_hDepth);
    }
}

void SkyboxPass::Execute(FrameGraphResources& resources, const RenderQueue& queue, RenderContext& rc) {
    if (rc.environment.skyboxPath.empty()) return;

    // �`�P�b�g��������
    ITexture* rtScene = resources.GetTexture(m_hSceneColor);
    ITexture* dsReal = resources.GetTexture(m_hDepth);

    if (!rtScene || !dsReal) return;

    // �X�J�C�{�b�N�X�擾
    IResourceFactory* factory = Graphics::Instance().GetResourceFactory();
    Skybox* skybox = Skybox::Get(factory, rc.environment.skyboxPath);

    static bool s_loggedOnce = false;
    if (!s_loggedOnce) {
        LOG_INFO("[SkyboxPass] path=%s skybox=%p", rc.environment.skyboxPath.c_str(), skybox);
        s_loggedOnce = true;
    }

    if (skybox) {
        // --- �`��X�e�[�g�̏��� ---
        // ���C�e�B���O���ʂ��������Ɂu�㏑���`��v���邽�� SetRenderTarget ���Ă�
        rc.commandList->TransitionBarrier(rtScene, ResourceState::RenderTarget);
        rc.commandList->TransitionBarrier(dsReal, ResourceState::DepthWrite);
        rc.commandList->SetRenderTarget(rtScene, dsReal);

        // �R���e�L�X�g�����X�V�iSkybox::Draw�����Ŏg�p�����\�������邽�߁j
        rc.mainRenderTarget = rtScene;
        rc.mainDepthStencil = dsReal;

        // �r���[�|�[�g�ݒ�
        rc.mainViewport = RhiViewport(0.0f, 0.0f, (float)rtScene->GetWidth(), (float)rtScene->GetHeight());
        rc.commandList->SetViewport(rc.mainViewport);

        // �s��v�Z (RenderContext�̐������̂��g�p)
        DirectX::XMMATRIX V = DirectX::XMLoadFloat4x4(&rc.viewMatrix);
        DirectX::XMMATRIX P = DirectX::XMLoadFloat4x4(&rc.projectionMatrix);

        // �X�J�C�{�b�N�X�̓J�����̈ړ��𖳎����邽�߁AView�s��̕��s�ړ�����������
        V.r[3] = DirectX::XMVectorSet(0, 0, 0, 1);

        DirectX::XMFLOAT4X4 vp;
        DirectX::XMStoreFloat4x4(&vp, V * P);

        // �`����s
        skybox->Draw(rc, vp);

        // ���Еt��
        rc.commandList->SetRenderTarget(nullptr, nullptr);
    }
}
