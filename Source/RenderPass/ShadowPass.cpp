#include "ShadowPass.h"
#include "Graphics.h"
#include "ShadowMap.h"
#include "Render/GlobalRootSignature.h"
#include "Render/ShaderCommon.h"
#include "RenderGraph/FrameGraphBuilder.h"
#include "RenderGraph/FrameGraphResources.h"
#include "RenderContext/IndirectDrawCommon.h"

void ShadowPass::Setup(FrameGraphBuilder& builder)
{
    m_hShadowMap = builder.GetHandle("ShadowMap");

    if (m_hShadowMap.IsValid()) {
        m_hShadowMap = builder.Write(m_hShadowMap);
    }
}

void ShadowPass::Execute(FrameGraphResources& resources, const RenderQueue& queue, RenderContext& rc) {
    auto shadowMap = const_cast<ShadowMap*>(rc.shadowMap);
    if (!shadowMap) return;

    // View/Proj のバックアップ (影描画はライトのView/Projを使うため)
    auto mainView = rc.viewMatrix;
    auto mainProj = rc.projectionMatrix;
    auto mainCamPos = rc.cameraPosition;
    auto mainCamDir = rc.cameraDirection;

    // 1. カスケード行列更新
    shadowMap->UpdateCascades(rc);

    {
        CbShadowMap shadow = {};
        for (int i = 0; i < ShadowMap::CASCADE_COUNT; ++i) {
            shadow.lightViewProjections[i] = shadowMap->GetLightViewProjection(i);
        }
        shadow.cascadeSplits = {
            shadowMap->GetCascadeEnd(0),
            shadowMap->GetCascadeEnd(1),
            shadowMap->GetCascadeEnd(2),
            0.0f
        };
        shadow.shadowColor = { rc.shadowColor.x, rc.shadowColor.y, rc.shadowColor.z, 1.0f };
        shadow.shadowBias = { 0.005f, 0.0f, 0.0f, 0.0f };
        rc.commandList->UpdateBuffer(GlobalRootSignature::Instance().GetShadowBuffer(), &shadow, sizeof(shadow));
    }

    // 2. 各カスケード描画
    for (int i = 0; i < ShadowMap::CASCADE_COUNT; ++i) {
        shadowMap->BeginCascade(rc, i);

        if (!rc.activeDrawCommands.empty() || !rc.activeSkinnedCommands.empty()) {
            for (const auto& cmd : rc.activeDrawCommands) {
                if (!cmd.modelResource || !cmd.key.castShadow) {
                    continue;
                }
                if (rc.activeInstanceBuffer && cmd.instanceCount > 0) {
                    uint32_t offsetBytes = cmd.drawArgsIndex * DRAW_ARGS_STRIDE;
                    shadowMap->DrawInstanced(
                        rc,
                        cmd.modelResource.get(),
                        static_cast<int>(cmd.meshIndex),
                        rc.activeInstanceBuffer,
                        rc.activeInstanceStride,
                        cmd.firstInstance,
                        cmd.instanceCount,
                        rc.activeDrawArgsBuffer,
                        offsetBytes);
                }
            }
            for (const auto& cmd : rc.activeSkinnedCommands) {
                if (!cmd.modelResource || !cmd.key.castShadow) {
                    continue;
                }
                const uint32_t begin = cmd.firstInstance;
                const uint32_t end = cmd.firstInstance + cmd.instanceCount;
                for (uint32_t j = begin; j < end && j < rc.preparedInstanceData.size(); ++j) {
                    shadowMap->Draw(rc, cmd.modelResource.get(), rc.preparedInstanceData[j].worldMatrix);
                }
            }
        } else {
            for (const auto& batch : queue.opaqueInstanceBatches) {
                if (!batch.modelResource || !batch.key.castShadow) {
                    continue;
                }
                for (const auto& instance : batch.instances) {
                    shadowMap->Draw(rc, batch.modelResource.get(), instance.worldMatrix);
                }
            }
        }
    }
    shadowMap->End(rc);
    if (shadowMap->GetTexture()) {
        rc.commandList->TransitionBarrier(shadowMap->GetTexture(), ResourceState::ShaderResource);
    }

    // View/Proj を復元 (後続の GBufferPass, LightingPass 用)
    rc.viewMatrix = mainView;
    rc.projectionMatrix = mainProj;
    rc.cameraPosition = mainCamPos;
    rc.cameraDirection = mainCamDir;
}
