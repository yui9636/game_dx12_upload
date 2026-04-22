#include "BuildIndirectCommandPass.h"
#include "Graphics.h"
#include "Model/ModelResource.h"
#include "RHI/IBuffer.h"
#include "RHI/IResourceFactory.h"
#include "RenderContext/IndirectDrawCommon.h"
#include "System/TaskSystem.h"
#include <chrono>
#include <cmath>
#include <vector>

namespace
{
    bool CanMergeGpuDrivenGroup(
        const RenderContext::PreparedIndirectCommand& first,
        const RenderContext::PreparedIndirectCommand& next,
        uint32_t expectedOffsetBytes)
    {
        return first.modelResource.get() == next.modelResource.get() &&
            first.meshIndex == next.meshIndex &&
            first.supportsInstancing == next.supportsInstancing &&
            first.key == next.key &&
            next.argumentOffsetBytes == expectedOffsetBytes;
    }
}

void BuildIndirectCommandPass::Setup(FrameGraphBuilder& builder, const RenderContext& rc)
{
    (void)builder;
}

void BuildIndirectCommandPass::Execute(FrameGraphResources& resources, const RenderQueue& queue, RenderContext& rc)
{
    using Clock = std::chrono::high_resolution_clock;
    const auto startTime = Clock::now();

    (void)resources;
    (void)queue;

    rc.activeDrawCommands.clear();
    rc.activeSkinnedCommands.clear();
    rc.preparedIndirectCommands.clear();
    rc.preparedSkinnedCommands.clear();
    rc.gpuDrivenDispatchGroups.clear();
    rc.prepMetrics.preparedIndirectCount = 0;
    rc.prepMetrics.preparedSkinnedCount = 0;
    rc.prepMetrics.gpuDrivenDispatchGroupCount = 0;
    rc.prepMetrics.gpuDrivenDispatchReduction = 0.0f;

    std::vector<BatchCommandBuildResult> batchResults;
    batchResults.resize(rc.preparedOpaqueInstanceBatches.size());

    TaskSystem::Instance().ParallelFor(
        rc.preparedOpaqueInstanceBatches.size(),
        1,
        [&](size_t batchIndex) {
            const auto& batch = rc.preparedOpaqueInstanceBatches[batchIndex];
            if (!batch.modelResource || batch.instanceCount == 0) {
                return;
            }

            auto& result = batchResults[batchIndex];
            result.drawCommands.clear();
            result.skinnedCommands.clear();
            result.drawArgs.clear();
            result.metadata.clear();
            result.preparedDrawCommands.clear();
            result.preparedSkinnedCommands.clear();
            result.drawCommands.reserve(batch.modelResource->GetMeshCount());
            result.skinnedCommands.reserve(batch.modelResource->GetMeshCount());
            result.drawArgs.reserve(batch.modelResource->GetMeshCount());
            result.metadata.reserve(batch.modelResource->GetMeshCount());
            result.preparedDrawCommands.reserve(batch.modelResource->GetMeshCount());
            result.preparedSkinnedCommands.reserve(batch.modelResource->GetMeshCount());

            for (uint32_t meshIndex = 0; meshIndex < static_cast<uint32_t>(batch.modelResource->GetMeshCount()); ++meshIndex) {
                const auto* meshResource = batch.modelResource->GetMeshResource(static_cast<int>(meshIndex));
                if (!meshResource) {
                    continue;
                }

                const auto& material = meshResource->material;
                if (material.alphaMode == Model::AlphaMode::Blend ||
                    (material.color.w > 0.01f && material.color.w < 0.99f)) {
                    continue;
                }

                const bool isInstancable = meshResource->bones.empty();

                IndirectDrawCommand cmd{};
                cmd.key = batch.key;
                cmd.modelResource = batch.modelResource;
                cmd.meshIndex = meshIndex;
                cmd.firstInstance = batch.firstInstance;
                cmd.instanceCount = batch.instanceCount;
                cmd.supportsInstancing = isInstancable;

                RenderContext::PreparedIndirectCommand prepared{};
                prepared.key = batch.key;
                prepared.modelResource = batch.modelResource;
                prepared.meshIndex = meshIndex;
                prepared.firstInstance = batch.firstInstance;
                prepared.instanceCount = batch.instanceCount;
                prepared.supportsInstancing = isInstancable;

                if (isInstancable) {
                    DrawArgs args{};
                    args.indexCountPerInstance = batch.modelResource->GetMeshIndexCount(static_cast<int>(meshIndex));
                    args.instanceCount = batch.instanceCount;
                    args.startIndexLocation = 0;
                    args.baseVertexLocation = 0;
                    args.startInstanceLocation = batch.firstInstance;
                    result.drawArgs.push_back(args);

                    RenderContext::GpuDrivenCommandMetadata meta{};
                    meta.firstInstance = batch.firstInstance;
                    meta.instanceCount = batch.instanceCount;
                    meta.outputInstanceStart = 0;
                    meta.drawArgsIndex = 0;
                    meta.indexCount = args.indexCountPerInstance;
                    meta.baseVertex = args.baseVertexLocation;
                    const auto bounds = batch.modelResource->GetLocalBounds();
                    meta.boundsCenterX = bounds.Center.x;
                    meta.boundsCenterY = bounds.Center.y;
                    meta.boundsCenterZ = bounds.Center.z;
                    meta.boundsRadius = static_cast<float>(std::sqrt(
                        bounds.Extents.x * bounds.Extents.x +
                        bounds.Extents.y * bounds.Extents.y +
                        bounds.Extents.z * bounds.Extents.z));
                    result.metadata.push_back(meta);

                    result.drawCommands.push_back(cmd);
                    result.preparedDrawCommands.push_back(prepared);
                } else {
                    result.skinnedCommands.push_back(cmd);
                    result.preparedSkinnedCommands.push_back(prepared);
                }
            }
        });

    std::vector<DrawArgs> drawArgs;
    std::vector<RenderContext::GpuDrivenCommandMetadata> metadata;
    size_t totalDrawCommands = 0;
    size_t totalSkinnedCommands = 0;
    size_t totalPreparedDrawCommands = 0;
    size_t totalPreparedSkinnedCommands = 0;
    size_t totalDrawArgs = 0;
    size_t totalMetadata = 0;
    for (const auto& result : batchResults) {
        totalDrawCommands += result.drawCommands.size();
        totalSkinnedCommands += result.skinnedCommands.size();
        totalPreparedDrawCommands += result.preparedDrawCommands.size();
        totalPreparedSkinnedCommands += result.preparedSkinnedCommands.size();
        totalDrawArgs += result.drawArgs.size();
        totalMetadata += result.metadata.size();
    }

    rc.activeDrawCommands.reserve(totalDrawCommands);
    rc.activeSkinnedCommands.reserve(totalSkinnedCommands);
    rc.preparedIndirectCommands.reserve(totalPreparedDrawCommands);
    rc.preparedSkinnedCommands.reserve(totalPreparedSkinnedCommands);
    drawArgs.reserve(totalDrawArgs);
    metadata.reserve(totalMetadata);

    uint32_t outputInstanceStart = 0;
    for (auto& result : batchResults) {
        const uint32_t drawArgsBaseIndex = static_cast<uint32_t>(drawArgs.size());
        const uint32_t metadataBaseIndex = static_cast<uint32_t>(metadata.size());

        drawArgs.insert(drawArgs.end(), result.drawArgs.begin(), result.drawArgs.end());
        metadata.insert(metadata.end(), result.metadata.begin(), result.metadata.end());

        for (size_t i = 0; i < result.drawCommands.size(); ++i) {
            result.drawCommands[i].drawArgsIndex = drawArgsBaseIndex + static_cast<uint32_t>(i);
            result.preparedDrawCommands[i].argumentOffsetBytes = (drawArgsBaseIndex + static_cast<uint32_t>(i)) * DRAW_ARGS_STRIDE;
            if (metadataBaseIndex + i < metadata.size()) {
                metadata[metadataBaseIndex + i].drawArgsIndex = drawArgsBaseIndex + static_cast<uint32_t>(i);
                metadata[metadataBaseIndex + i].outputInstanceStart = outputInstanceStart;
                outputInstanceStart += result.drawCommands[i].instanceCount;
            }
        }

        rc.activeDrawCommands.insert(rc.activeDrawCommands.end(), result.drawCommands.begin(), result.drawCommands.end());
        rc.activeSkinnedCommands.insert(rc.activeSkinnedCommands.end(), result.skinnedCommands.begin(), result.skinnedCommands.end());
        rc.preparedIndirectCommands.insert(rc.preparedIndirectCommands.end(), result.preparedDrawCommands.begin(), result.preparedDrawCommands.end());
        rc.preparedSkinnedCommands.insert(rc.preparedSkinnedCommands.end(), result.preparedSkinnedCommands.begin(), result.preparedSkinnedCommands.end());
    }

    rc.prepMetrics.preparedIndirectCount = static_cast<uint32_t>(rc.activeDrawCommands.size());
    rc.prepMetrics.preparedSkinnedCount = static_cast<uint32_t>(rc.activeSkinnedCommands.size());
    rc.prepMetrics.nonSkinnedCommandCount = static_cast<uint32_t>(rc.activeDrawCommands.size());
    rc.prepMetrics.skinnedCommandCount = static_cast<uint32_t>(rc.activeSkinnedCommands.size());
    rc.prepMetrics.gpuDrivenCandidateBatchCount = static_cast<uint32_t>(rc.activeDrawCommands.size());
    rc.prepMetrics.gpuDrivenCandidateInstanceCount = 0;
    for (const auto& cmd : rc.activeDrawCommands) {
        rc.prepMetrics.gpuDrivenCandidateInstanceCount += cmd.instanceCount;
    }

    for (size_t i = 0; i < rc.preparedIndirectCommands.size();) {
        const auto& first = rc.preparedIndirectCommands[i];
        RenderContext::GpuDrivenDispatchGroup group{};
        group.key = first.key;
        group.modelResource = first.modelResource;
        group.meshIndex = first.meshIndex;
        group.firstCommand = static_cast<uint32_t>(i);
        group.commandCount = 1;
        group.firstArgumentOffsetBytes = first.argumentOffsetBytes;
        group.supportsInstancing = first.supportsInstancing;

        uint32_t expectedOffsetBytes = first.argumentOffsetBytes + DRAW_ARGS_STRIDE;
        ++i;
        while (i < rc.preparedIndirectCommands.size() &&
               CanMergeGpuDrivenGroup(first, rc.preparedIndirectCommands[i], expectedOffsetBytes)) {
            ++group.commandCount;
            expectedOffsetBytes += DRAW_ARGS_STRIDE;
            ++i;
        }

        rc.gpuDrivenDispatchGroups.push_back(std::move(group));
    }
    rc.prepMetrics.gpuDrivenDispatchGroupCount = static_cast<uint32_t>(rc.gpuDrivenDispatchGroups.size());
    if (!rc.preparedIndirectCommands.empty()) {
        rc.prepMetrics.gpuDrivenDispatchReduction =
            1.0f - (static_cast<float>(rc.gpuDrivenDispatchGroups.size()) /
                    static_cast<float>(rc.preparedIndirectCommands.size()));
    }

    const uint32_t requiredArgsBytes = static_cast<uint32_t>(drawArgs.size() * sizeof(DrawArgs));
    const uint32_t requiredMetadataBytes = static_cast<uint32_t>(metadata.size() * sizeof(RenderContext::GpuDrivenCommandMetadata));
    if (requiredArgsBytes == 0) {
        rc.activeDrawArgsBuffer = nullptr;
        rc.prepMetrics.indirectBuildMs =
            std::chrono::duration<double, std::milli>(Clock::now() - startTime).count();
        return;
    }

    auto* factory = Graphics::Instance().GetResourceFactory();
    if (!factory) {
        rc.activeDrawCommands.clear();
        rc.activeSkinnedCommands.clear();
        rc.preparedIndirectCommands.clear();
        rc.preparedSkinnedCommands.clear();
        rc.gpuDrivenDispatchGroups.clear();
        rc.activeDrawArgsBuffer = nullptr;
        rc.prepMetrics.preparedIndirectCount = 0;
        rc.prepMetrics.preparedSkinnedCount = 0;
        rc.prepMetrics.gpuDrivenDispatchGroupCount = 0;
        rc.prepMetrics.gpuDrivenDispatchReduction = 0.0f;
        rc.prepMetrics.indirectBuildMs =
            std::chrono::duration<double, std::milli>(Clock::now() - startTime).count();
        return;
    }

    if (!rc.preparedIndirectArgumentBuffer || rc.preparedIndirectArgumentCapacity < requiredArgsBytes) {
        constexpr uint32_t kAlignment = 256;
        const uint32_t capacity = (requiredArgsBytes + (kAlignment - 1)) & ~(kAlignment - 1);
        rc.preparedIndirectArgumentBuffer = std::shared_ptr<IBuffer>(
            factory->CreateBuffer(capacity, BufferType::Indirect, nullptr).release());
        rc.preparedIndirectArgumentCapacity = rc.preparedIndirectArgumentBuffer ? capacity : 0;
        ++rc.prepMetrics.indirectBufferReallocs;
    }

    if (!rc.preparedIndirectCommandMetadataBuffer ||
        rc.preparedIndirectCommandMetadataCapacity < requiredMetadataBytes ||
        rc.preparedIndirectCommandMetadataBuffer->GetSize() < requiredMetadataBytes) {
        const uint32_t capacity = requiredMetadataBytes;
        rc.preparedIndirectCommandMetadataBuffer = std::shared_ptr<IBuffer>(
            factory->CreateStructuredBuffer(sizeof(RenderContext::GpuDrivenCommandMetadata), static_cast<uint32_t>(metadata.size()), nullptr).release());
        rc.preparedIndirectCommandMetadataCapacity = rc.preparedIndirectCommandMetadataBuffer ? capacity : 0;
    }

    if (rc.commandList && rc.preparedIndirectArgumentBuffer) {
        rc.commandList->UpdateBuffer(
            rc.preparedIndirectArgumentBuffer.get(),
            drawArgs.data(),
            requiredArgsBytes);
    }

    if (rc.commandList && rc.preparedIndirectCommandMetadataBuffer && !metadata.empty()) {
        rc.commandList->UpdateBuffer(
            rc.preparedIndirectCommandMetadataBuffer.get(),
            metadata.data(),
            requiredMetadataBytes);
    }

    rc.activeInstanceBuffer = rc.preparedInstanceBuffer.get();
    rc.activeInstanceStride = rc.preparedInstanceStride;
    rc.activeDrawArgsBuffer = rc.preparedIndirectArgumentBuffer.get();
    rc.prepMetrics.indirectBuildMs =
        std::chrono::duration<double, std::milli>(Clock::now() - startTime).count();
}
