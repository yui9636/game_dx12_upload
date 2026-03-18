#include "ForwardTransparentPass.h"
#include "Graphics.h"
#include "Model/ModelRenderer.h"
#include "RHI/ICommandList.h"
#include "RHI/ITexture.h"
#include "RenderGraph/FrameGraphBuilder.h"
#include "RenderGraph/FrameGraphResources.h"

void ForwardTransparentPass::Setup(FrameGraphBuilder& builder)
{
    // 1. �f������n���h�����擾
    m_hSceneColor = builder.GetHandle("SceneColor");
    m_hDepth = builder.GetHandle("GBufferDepth");

    // 2. �g�p�錾
    // ��������`�����ސ�iSceneColor�j�͏������݁A
    // �Օ�����Ɏg���[�x�iGBufferDepth�j�͓ǂݍ��݁B
    if (m_hSceneColor.IsValid()) {
        builder.Read(m_hSceneColor);
    }
    if (m_hDepth.IsValid()) {
        builder.Read(m_hDepth);
    }
}

void ForwardTransparentPass::Execute(FrameGraphResources& resources, const RenderQueue& queue, RenderContext& rc) {
    // �O���t���畨���e�N�X�`��������������
    ITexture* rtScene = resources.GetTexture(m_hSceneColor);
    ITexture* dsReal = resources.GetTexture(m_hDepth);

    if (!rtScene || !dsReal) return;

    Graphics& g = Graphics::Instance();

    // 1. �`�����Z�b�g�i�O���t�Ǘ����̃e�N�X�`�����o�C���h�j
    rc.commandList->TransitionBarrier(rtScene, ResourceState::RenderTarget);
    rc.commandList->TransitionBarrier(dsReal, ResourceState::DepthWrite);
    rc.commandList->SetRenderTarget(rtScene, dsReal);

    // �R���e�L�X�g����
    rc.mainRenderTarget = rtScene;
    rc.mainDepthStencil = dsReal;
    rc.mainViewport = RhiViewport(0.0f, 0.0f, (float)rtScene->GetWidth(), (float)rtScene->GetHeight());
    rc.commandList->SetViewport(rc.mainViewport);

    auto renderer = g.GetModelRenderer();
    if (!renderer) return;

    // IBL�Ȃǂ̊������Z�b�g
    renderer->SetIBL(rc.environment.diffuseIBLPath, rc.environment.specularIBLPath);

    // 2. �`�[���甼�����p�P�b�g��o�^
    for (const auto& packet : queue.transparentPackets) {
        if (!packet.model) continue;

        // ���L���������Ȃ� shared_ptr �Ƃ��ă��b�v�iModelRenderer�̈����ɍ��킹��j
        std::shared_ptr<Model> sharedModel(packet.model, [](Model*) {});

        renderer->Draw(
            static_cast<ShaderId>(packet.shaderId), sharedModel, packet.worldMatrix, packet.prevWorldMatrix,
            packet.baseColor, packet.metallic, packet.roughness, packet.emissive,
            packet.blendState, packet.depthState, packet.rasterizerState
        );
    }

    // 3. �������I�u�W�F�N�g����C�ɕ`��I
    renderer->RenderTransparent(rc);

    // ���Еt��
    rc.commandList->SetRenderTarget(nullptr, nullptr);
}
