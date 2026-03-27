#include "BuildInstanceBufferPass.h"
#include "Graphics.h"
#include "RHI/IResourceFactory.h"
#include "RHI/IBuffer.h"
#include "System/TaskSystem.h"
#include <algorithm>
#include <chrono>

void BuildInstanceBufferPass::Setup(FrameGraphBuilder& builder, const RenderContext& rc)
{
    (void)builder;
}

void BuildInstanceBufferPass::Execute(FrameGraphResources& resources, const RenderQueue& queue, RenderContext& rc)
{
    using Clock = std::chrono::high_resolution_clock;
    const auto startTime = Clock::now();

    // 可視 instance を連続した配列へ詰め替え、instance stream と structured buffer を更新する。
    (void)resources;
    (void)queue;

    rc.preparedInstanceData.clear();
    rc.preparedOpaqueInstanceBatches.clear();
    rc.preparedVisibleInstanceCount = 0;
    rc.useGpuCulling = false; // Reset; ComputeCullingPass will set if active

    // BuildIndirectCommandPass が埋める active フィールドを先に初期化する
    rc.activeInstanceBuffer = nullptr;
    rc.activeDrawArgsBuffer = nullptr;
    rc.activeDrawCommands.clear();
    rc.activeSkinnedCommands.clear();

    // batch ごとに instance 範囲を記録して、後段の indirect command から参照できるようにする。
    uint32_t firstInstance = 0;
    uint32_t totalInstanceCount = 0;
    if (m_batchScratch.capacity() < rc.visibleOpaqueInstanceBatches.size()) {
        ++rc.prepMetrics.preparedBatchVectorGrowths;
    }
    m_batchScratch.clear();
    m_batchScratch.reserve(rc.visibleOpaqueInstanceBatches.size());
    for (const auto& batch : rc.visibleOpaqueInstanceBatches) {
        RenderContext::PreparedInstanceBatch prepared{};
        prepared.key = batch.key;
        prepared.modelResource = batch.modelResource;
        prepared.firstInstance = firstInstance;
        prepared.instanceCount = static_cast<uint32_t>(batch.instances.size());
        m_batchScratch.push_back(prepared);
        firstInstance += prepared.instanceCount;
        totalInstanceCount += prepared.instanceCount;
    }

    if (m_instanceScratch.capacity() < totalInstanceCount) {
        ++rc.prepMetrics.preparedInstanceVectorGrowths;
    }
    m_instanceScratch.resize(totalInstanceCount);
    TaskSystem::Instance().ParallelFor(
        rc.visibleOpaqueInstanceBatches.size(),
        1,
        [&](size_t batchIndex) {
            const auto& batch = rc.visibleOpaqueInstanceBatches[batchIndex];
            const auto& prepared = m_batchScratch[batchIndex];
            if (batch.instances.empty()) {
                return;
            }

            std::copy(
                batch.instances.begin(),
                batch.instances.end(),
                m_instanceScratch.begin() + prepared.firstInstance);
        });

    rc.preparedOpaqueInstanceBatches = m_batchScratch;
    rc.preparedInstanceData = m_instanceScratch;

    rc.preparedInstanceStride = sizeof(InstanceData);
    rc.preparedVisibleInstanceCount = static_cast<uint32_t>(rc.preparedInstanceData.size());

    const uint32_t requiredBytes = static_cast<uint32_t>(rc.preparedInstanceData.size() * sizeof(InstanceData));

    if (requiredBytes == 0) {
        rc.activeInstanceBuffer = nullptr;
        rc.preparedVisibleInstanceCount = 0;
        rc.prepMetrics.preparedBatchCount = static_cast<uint32_t>(rc.preparedOpaqueInstanceBatches.size());
        rc.prepMetrics.instanceBuildMs =
            std::chrono::duration<double, std::milli>(Clock::now() - startTime).count();
        return;
    }

    auto* factory = Graphics::Instance().GetResourceFactory();
    if (!factory) {
        rc.activeInstanceBuffer = nullptr;
        rc.preparedVisibleInstanceCount = 0;
        rc.prepMetrics.instanceBuildMs =
            std::chrono::duration<double, std::milli>(Clock::now() - startTime).count();
        return;
    }

    if (!rc.preparedInstanceBuffer || rc.preparedInstanceCapacity < requiredBytes) {
        constexpr uint32_t kAlignment = 256;
        const uint32_t capacity = (requiredBytes + (kAlignment - 1)) & ~(kAlignment - 1);
        rc.preparedInstanceBuffer = std::shared_ptr<IBuffer>(
            factory->CreateBuffer(capacity, BufferType::Vertex, nullptr).release());
        rc.preparedVisibleInstanceStructuredBuffer = std::shared_ptr<IBuffer>(
            factory->CreateStructuredBuffer(sizeof(InstanceData), rc.preparedVisibleInstanceCount, nullptr).release());
        rc.preparedInstanceCapacity = rc.preparedInstanceBuffer ? capacity : 0;
        ++rc.prepMetrics.instanceBufferReallocs;
        ++rc.prepMetrics.visibleStructuredBufferReallocs;
    }
    else if (!rc.preparedVisibleInstanceStructuredBuffer || rc.preparedVisibleInstanceStructuredBuffer->GetSize() < requiredBytes) {
        rc.preparedVisibleInstanceStructuredBuffer = std::shared_ptr<IBuffer>(
            factory->CreateStructuredBuffer(sizeof(InstanceData), rc.preparedVisibleInstanceCount, nullptr).release());
        ++rc.prepMetrics.visibleStructuredBufferReallocs;
    }

    if (rc.commandList) {
        if (rc.preparedInstanceBuffer) {
            rc.commandList->UpdateBuffer(
                rc.preparedInstanceBuffer.get(),
                rc.preparedInstanceData.data(),
                requiredBytes);
        }
        if (rc.preparedVisibleInstanceStructuredBuffer) {
            rc.commandList->UpdateBuffer(
                rc.preparedVisibleInstanceStructuredBuffer.get(),
                rc.preparedInstanceData.data(),
                requiredBytes);
        }
    }

    rc.prepMetrics.preparedBatchCount = static_cast<uint32_t>(rc.preparedOpaqueInstanceBatches.size());
    rc.prepMetrics.instanceBuildMs =
        std::chrono::duration<double, std::milli>(Clock::now() - startTime).count();
}
