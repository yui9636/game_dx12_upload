#include "BuildInstanceBufferPass.h"
#include "Graphics.h"
#include "RHI/IResourceFactory.h"
#include "RHI/IBuffer.h"

void BuildInstanceBufferPass::Setup(FrameGraphBuilder& builder)
{
    (void)builder;
}

void BuildInstanceBufferPass::Execute(FrameGraphResources& resources, const RenderQueue& queue, RenderContext& rc)
{
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
    for (const auto& batch : rc.visibleOpaqueInstanceBatches) {
        RenderContext::PreparedInstanceBatch prepared{};
        prepared.key = batch.key;
        prepared.modelResource = batch.modelResource;
        prepared.firstInstance = firstInstance;
        prepared.instanceCount = static_cast<uint32_t>(batch.instances.size());
        rc.preparedOpaqueInstanceBatches.push_back(prepared);

        rc.preparedInstanceData.insert(
            rc.preparedInstanceData.end(),
            batch.instances.begin(),
            batch.instances.end());

        firstInstance += prepared.instanceCount;
    }

    rc.preparedInstanceStride = sizeof(InstanceData);
    rc.preparedVisibleInstanceCount = static_cast<uint32_t>(rc.preparedInstanceData.size());

    const uint32_t requiredBytes = static_cast<uint32_t>(rc.preparedInstanceData.size() * sizeof(InstanceData));

    if (requiredBytes == 0) {
        rc.preparedInstanceBuffer.reset();
        rc.preparedVisibleInstanceStructuredBuffer.reset();
        rc.preparedInstanceCapacity = 0;
        return;
    }

    auto* factory = Graphics::Instance().GetResourceFactory();
    if (!factory) {
        rc.preparedInstanceBuffer.reset();
        rc.preparedVisibleInstanceStructuredBuffer.reset();
        rc.preparedInstanceCapacity = 0;
        rc.preparedVisibleInstanceCount = 0;
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
    } else if (!rc.preparedVisibleInstanceStructuredBuffer || rc.preparedVisibleInstanceStructuredBuffer->GetSize() < requiredBytes) {
        rc.preparedVisibleInstanceStructuredBuffer = std::shared_ptr<IBuffer>(
            factory->CreateStructuredBuffer(sizeof(InstanceData), rc.preparedVisibleInstanceCount, nullptr).release());
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
}
