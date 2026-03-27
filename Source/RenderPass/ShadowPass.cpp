#include "ShadowPass.h"
#include "Graphics.h"
#include "ShadowMap.h"
#include "Render/GlobalRootSignature.h"
#include "Render/ShaderCommon.h"
#include "RenderGraph/FrameGraphBuilder.h"
#include "RenderGraph/FrameGraphResources.h"
#include "RenderContext/IndirectDrawCommon.h"
#include "RHI/DX12/DX12Device.h"
#include "Console/Logger.h"

void ShadowPass::Setup(FrameGraphBuilder& builder, const RenderContext& rc)
{
    m_hShadowMap = builder.GetHandle("ShadowMap");

    if (m_hShadowMap.IsValid()) {
        m_hShadowMap = builder.Write(m_hShadowMap);
    }
}

void ShadowPass::Execute(FrameGraphResources& resources, const RenderQueue& queue, RenderContext& rc) {
    if (Graphics::Instance().GetAPI() == GraphicsAPI::DX12 && rc.pendingAsyncComputeFenceValue != 0) {
        if (auto* dx12Device = Graphics::Instance().GetDX12Device()) {
            dx12Device->QueueGraphicsWaitForCompute(rc.pendingAsyncComputeFenceValue);
            rc.prepMetrics.asyncComputeWaitCount++;
        }
        rc.pendingAsyncComputeFenceValue = 0;
    }

    auto shadowMap = const_cast<ShadowMap*>(rc.shadowMap);
    if (!shadowMap) return;

    auto mainView = rc.viewMatrix;
    auto mainProj = rc.projectionMatrix;
    auto mainCamPos = rc.cameraPosition;
    auto mainCamDir = rc.cameraDirection;

    shadowMap->UpdateCascades(rc);

    uint32_t instancedGroupDraws = 0;
    uint32_t instancedCommandDraws = 0;
    uint32_t skinnedDraws = 0;
    uint32_t fallbackBatchDraws = 0;

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
        shadow.shadowBias = { 0.00002f, 0.0f, 0.0f, 0.0f };
        IBuffer* shadowBuffer = rc.shadowConstantBufferOverride
            ? rc.shadowConstantBufferOverride
            : GlobalRootSignature::Instance().GetShadowBuffer();
        rc.commandList->UpdateBuffer(shadowBuffer, &shadow, sizeof(shadow));
    }

    for (int i = 0; i < ShadowMap::CASCADE_COUNT; ++i) {
        shadowMap->BeginCascade(rc, i);

        if (!rc.activeDrawCommands.empty() || !rc.activeSkinnedCommands.empty()) {
            if (rc.activeInstanceBuffer && rc.activeDrawArgsBuffer && !rc.gpuDrivenDispatchGroups.empty()) {
                for (const auto& group : rc.gpuDrivenDispatchGroups) {
                    if (!group.modelResource || !group.key.castShadow || !group.supportsInstancing) {
                        continue;
                    }
                    ++instancedGroupDraws;
                    shadowMap->DrawInstancedMulti(
                        rc,
                        group.modelResource.get(),
                        static_cast<int>(group.meshIndex),
                        rc.activeInstanceBuffer,
                        rc.activeInstanceStride,
                        rc.activeDrawArgsBuffer,
                        group.firstArgumentOffsetBytes,
                        group.commandCount,
                        DRAW_ARGS_STRIDE);
                }
            } else {
                for (const auto& cmd : rc.activeDrawCommands) {
                    if (!cmd.modelResource || !cmd.key.castShadow) {
                        continue;
                    }
                    if (rc.activeInstanceBuffer && cmd.instanceCount > 0) {
                        ++instancedCommandDraws;
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
            }
            for (const auto& cmd : rc.activeSkinnedCommands) {
                if (!cmd.modelResource || !cmd.key.castShadow) {
                    continue;
                }
                const uint32_t begin = cmd.firstInstance;
                const uint32_t end = cmd.firstInstance + cmd.instanceCount;
                for (uint32_t j = begin; j < end && j < rc.preparedInstanceData.size(); ++j) {
                    ++skinnedDraws;
                    shadowMap->Draw(rc, cmd.modelResource.get(), rc.preparedInstanceData[j].worldMatrix);
                }
            }
        } else {
            for (const auto& batch : queue.opaqueInstanceBatches) {
                if (!batch.modelResource || !batch.key.castShadow) {
                    continue;
                }
                for (const auto& instance : batch.instances) {
                    ++fallbackBatchDraws;
                    shadowMap->Draw(rc, batch.modelResource.get(), instance.worldMatrix);
                }
            }
        }
    }
    shadowMap->End(rc);
    if (shadowMap->GetTexture()) {
        rc.commandList->TransitionBarrier(shadowMap->GetTexture(), ResourceState::ShaderResource);
    }

    rc.viewMatrix = mainView;
    rc.projectionMatrix = mainProj;
    rc.cameraPosition = mainCamPos;
    rc.cameraDirection = mainCamDir;

    static uint32_t s_shadowLogCount = 0;
    if (s_shadowLogCount < 8) {
        ++s_shadowLogCount;
        LOG_INFO(
            "[ShadowPassStats] frame=%u groups=%u instanced=%u skinned=%u fallback=%u activeDraw=%u activeSkinned=%u batches=%u dir=(%.3f,%.3f,%.3f) splits=(%.3f,%.3f,%.3f)",
            s_shadowLogCount - 1,
            instancedGroupDraws,
            instancedCommandDraws,
            skinnedDraws,
            fallbackBatchDraws,
            static_cast<uint32_t>(rc.activeDrawCommands.size()),
            static_cast<uint32_t>(rc.activeSkinnedCommands.size()),
            queue.metrics.opaqueBatchCount,
            rc.directionalLight.direction.x,
            rc.directionalLight.direction.y,
            rc.directionalLight.direction.z,
            shadowMap->GetCascadeEnd(0),
            shadowMap->GetCascadeEnd(1),
            shadowMap->GetCascadeEnd(2));
    }
}
