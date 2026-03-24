#include "BuildIndirectCommandPass.h"
#include "Graphics.h"
#include "Model/ModelResource.h"
#include "RHI/IBuffer.h"
#include "RHI/IResourceFactory.h"
#include "RenderContext/IndirectDrawCommon.h"
#include "System/TaskSystem.h"
#include <chrono>
#include <vector>

void BuildIndirectCommandPass::Setup(FrameGraphBuilder& builder)
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
    rc.prepMetrics.preparedIndirectCount = 0;
    rc.prepMetrics.preparedSkinnedCount = 0;

    if (m_batchResults.capacity() < rc.preparedOpaqueInstanceBatches.size()) {
        ++rc.prepMetrics.indirectScratchVectorGrowths;
    }
    m_batchResults.clear();
    m_batchResults.resize(rc.preparedOpaqueInstanceBatches.size());

    TaskSystem::Instance().ParallelFor(
        rc.preparedOpaqueInstanceBatches.size(),
        1,
        [&](size_t batchIndex) {
            const auto& batch = rc.preparedOpaqueInstanceBatches[batchIndex];
            if (!batch.modelResource || batch.instanceCount == 0) {
                return;
            }

            auto& result = m_batchResults[batchIndex];
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
                    meta.meshIndex = meshIndex;
                    meta.firstInstance = batch.firstInstance;
                    meta.instanceCount = batch.instanceCount;
                    meta.supportsInstancing = 1;
                    result.metadata.push_back(meta);

                    result.drawCommands.push_back(cmd);
                    result.preparedDrawCommands.push_back(prepared);
                } else {
                    result.skinnedCommands.push_back(cmd);
                    result.preparedSkinnedCommands.push_back(prepared);
                }
            }
        });

    auto& drawArgs = m_drawArgsScratch;
    auto& metadata = m_metadataScratch;
    drawArgs.clear();
    metadata.clear();
    size_t totalDrawCommands = 0;
    size_t totalSkinnedCommands = 0;
    size_t totalPreparedDrawCommands = 0;
    size_t totalPreparedSkinnedCommands = 0;
    size_t totalDrawArgs = 0;
    size_t totalMetadata = 0;
    for (const auto& result : m_batchResults) {
        totalDrawCommands += result.drawCommands.size();
        totalSkinnedCommands += result.skinnedCommands.size();
        totalPreparedDrawCommands += result.preparedDrawCommands.size();
        totalPreparedSkinnedCommands += result.preparedSkinnedCommands.size();
        totalDrawArgs += result.drawArgs.size();
        totalMetadata += result.metadata.size();
    }

    if (drawArgs.capacity() < totalDrawArgs) {
        ++rc.prepMetrics.drawArgsVectorGrowths;
    }
    if (metadata.capacity() < totalMetadata) {
        ++rc.prepMetrics.metadataVectorGrowths;
    }
    rc.activeDrawCommands.reserve(totalDrawCommands);
    rc.activeSkinnedCommands.reserve(totalSkinnedCommands);
    rc.preparedIndirectCommands.reserve(totalPreparedDrawCommands);
    rc.preparedSkinnedCommands.reserve(totalPreparedSkinnedCommands);
    drawArgs.reserve(totalDrawArgs);
    metadata.reserve(totalMetadata);

    for (auto& result : m_batchResults) {
        const uint32_t drawArgsBaseIndex = static_cast<uint32_t>(drawArgs.size());
        const uint32_t metadataBaseIndex = static_cast<uint32_t>(metadata.size());

        drawArgs.insert(drawArgs.end(), result.drawArgs.begin(), result.drawArgs.end());
        metadata.insert(metadata.end(), result.metadata.begin(), result.metadata.end());

        for (size_t i = 0; i < result.drawCommands.size(); ++i) {
            result.drawCommands[i].drawArgsIndex = drawArgsBaseIndex + static_cast<uint32_t>(i);
            result.preparedDrawCommands[i].argumentOffsetBytes = (drawArgsBaseIndex + static_cast<uint32_t>(i)) * DRAW_ARGS_STRIDE;
            if (metadataBaseIndex + i < metadata.size()) {
                metadata[metadataBaseIndex + i].argumentOffsetBytes = result.preparedDrawCommands[i].argumentOffsetBytes;
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

    const uint32_t requiredArgsBytes = static_cast<uint32_t>(drawArgs.size() * sizeof(DrawArgs));
    const uint32_t requiredMetadataBytes = static_cast<uint32_t>(metadata.size() * sizeof(RenderContext::GpuDrivenCommandMetadata));
    if (requiredArgsBytes == 0) {
        rc.preparedIndirectArgumentBuffer.reset();
        rc.preparedIndirectArgumentCapacity = 0;
        rc.preparedIndirectCommandMetadataBuffer.reset();
        rc.preparedIndirectCommandMetadataCapacity = 0;
        rc.activeDrawArgsBuffer = nullptr;
        rc.prepMetrics.indirectBuildMs =
            std::chrono::duration<double, std::milli>(Clock::now() - startTime).count();
        return;
    }

    auto* factory = Graphics::Instance().GetResourceFactory();
    if (!factory) {
        rc.preparedIndirectArgumentBuffer.reset();
        rc.preparedIndirectArgumentCapacity = 0;
        rc.preparedIndirectCommandMetadataBuffer.reset();
        rc.preparedIndirectCommandMetadataCapacity = 0;
        rc.activeDrawCommands.clear();
        rc.activeSkinnedCommands.clear();
        rc.preparedIndirectCommands.clear();
        rc.activeDrawArgsBuffer = nullptr;
        rc.prepMetrics.preparedIndirectCount = 0;
        rc.prepMetrics.preparedSkinnedCount = 0;
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

    if (!rc.preparedIndirectCommandMetadataBuffer || rc.preparedIndirectCommandMetadataCapacity < requiredMetadataBytes) {
        constexpr uint32_t kAlignment = 256;
        const uint32_t capacity = (requiredMetadataBytes + (kAlignment - 1)) & ~(kAlignment - 1);
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
