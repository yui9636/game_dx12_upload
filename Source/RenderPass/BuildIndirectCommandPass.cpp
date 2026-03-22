#include "BuildIndirectCommandPass.h"
#include "Graphics.h"
#include "Model/ModelResource.h"
#include "RHI/IBuffer.h"
#include "RHI/IResourceFactory.h"
#include "RenderContext/IndirectDrawCommon.h"

void BuildIndirectCommandPass::Setup(FrameGraphBuilder& builder)
{
    (void)builder;
}

void BuildIndirectCommandPass::Execute(FrameGraphResources& resources, const RenderQueue& queue, RenderContext& rc)
{
    (void)resources;
    (void)queue;

    // Clear active fields
    rc.activeDrawCommands.clear();
    rc.activeSkinnedCommands.clear();

    // Clear legacy fields (backward compatibility)
    rc.preparedIndirectCommands.clear();
    rc.preparedSkinnedCommands.clear();

    std::vector<DrawArgs> drawArgs;
    drawArgs.reserve(rc.preparedOpaqueInstanceBatches.size() * 4);

    for (const auto& batch : rc.preparedOpaqueInstanceBatches) {
        if (!batch.modelResource || batch.instanceCount == 0) {
            continue;
        }

        for (uint32_t meshIndex = 0; meshIndex < static_cast<uint32_t>(batch.modelResource->GetMeshCount()); ++meshIndex) {
            const auto* meshResource = batch.modelResource->GetMeshResource(static_cast<int>(meshIndex));
            if (!meshResource) {
                continue;
            }

            const auto& material = meshResource->material;
            if (material.alphaMode == Model::AlphaMode::Blend || (material.color.w > 0.01f && material.color.w < 0.99f)) {
                continue;
            }

            const bool isInstancable = meshResource->bones.empty();

            // New active* command
            IndirectDrawCommand cmd{};
            cmd.key = batch.key;
            cmd.modelResource = batch.modelResource;
            cmd.meshIndex = meshIndex;
            cmd.firstInstance = batch.firstInstance;
            cmd.instanceCount = batch.instanceCount;
            cmd.supportsInstancing = isInstancable;

            if (isInstancable) {
                cmd.drawArgsIndex = static_cast<uint32_t>(drawArgs.size());

                DrawArgs args{};
                args.indexCountPerInstance = batch.modelResource->GetMeshIndexCount(static_cast<int>(meshIndex));
                args.instanceCount = batch.instanceCount;
                args.startIndexLocation = 0;
                args.baseVertexLocation = 0;
                args.startInstanceLocation = batch.firstInstance;
                drawArgs.push_back(args);

                rc.activeDrawCommands.push_back(cmd);

                // Legacy field (backward compat)
                RenderContext::PreparedIndirectCommand oldCmd{};
                oldCmd.key = batch.key;
                oldCmd.modelResource = batch.modelResource;
                oldCmd.meshIndex = meshIndex;
                oldCmd.firstInstance = batch.firstInstance;
                oldCmd.instanceCount = batch.instanceCount;
                oldCmd.supportsInstancing = true;
                oldCmd.argumentOffsetBytes = cmd.drawArgsIndex * DRAW_ARGS_STRIDE;
                rc.preparedIndirectCommands.push_back(oldCmd);
            } else {
                rc.activeSkinnedCommands.push_back(cmd);

                // Legacy field (backward compat)
                RenderContext::PreparedIndirectCommand oldCmd{};
                oldCmd.key = batch.key;
                oldCmd.modelResource = batch.modelResource;
                oldCmd.meshIndex = meshIndex;
                oldCmd.firstInstance = batch.firstInstance;
                oldCmd.instanceCount = batch.instanceCount;
                oldCmd.supportsInstancing = false;
                rc.preparedSkinnedCommands.push_back(oldCmd);
            }
        }
    }

    const uint32_t requiredBytes = static_cast<uint32_t>(drawArgs.size() * sizeof(DrawArgs));
    if (requiredBytes == 0) {
        rc.preparedIndirectArgumentBuffer.reset();
        rc.preparedIndirectArgumentCapacity = 0;
        rc.activeDrawArgsBuffer = nullptr;
        return;
    }

    auto* factory = Graphics::Instance().GetResourceFactory();
    if (!factory) {
        rc.preparedIndirectArgumentBuffer.reset();
        rc.preparedIndirectArgumentCapacity = 0;
        rc.activeDrawCommands.clear();
        rc.activeSkinnedCommands.clear();
        rc.preparedIndirectCommands.clear();
        rc.activeDrawArgsBuffer = nullptr;
        return;
    }

    if (!rc.preparedIndirectArgumentBuffer || rc.preparedIndirectArgumentCapacity < requiredBytes) {
        constexpr uint32_t kAlignment = 256;
        const uint32_t capacity = (requiredBytes + (kAlignment - 1)) & ~(kAlignment - 1);
        rc.preparedIndirectArgumentBuffer = std::shared_ptr<IBuffer>(
            factory->CreateBuffer(capacity, BufferType::Indirect, nullptr).release());
        rc.preparedIndirectArgumentCapacity = rc.preparedIndirectArgumentBuffer ? capacity : 0;
    }

    if (rc.commandList && rc.preparedIndirectArgumentBuffer) {
        rc.commandList->UpdateBuffer(
            rc.preparedIndirectArgumentBuffer.get(),
            drawArgs.data(),
            requiredBytes);
    }

    // Set active fields for downstream passes
    rc.activeInstanceBuffer = rc.preparedInstanceBuffer.get();
    rc.activeInstanceStride = rc.preparedInstanceStride;
    rc.activeDrawArgsBuffer = rc.preparedIndirectArgumentBuffer.get();
}
