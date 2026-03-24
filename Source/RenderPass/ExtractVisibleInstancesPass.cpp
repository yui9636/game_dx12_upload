#include "ExtractVisibleInstancesPass.h"
#include "Model/ModelResource.h"
#include "System/TaskSystem.h"
#include <DirectXCollision.h>
#include <DirectXMath.h>
#include <chrono>
#include <vector>

namespace
{
    DirectX::BoundingFrustum BuildWorldFrustum(const RenderContext& rc)
    {
        using namespace DirectX;

        BoundingFrustum viewFrustum{};
        const XMMATRIX proj = XMLoadFloat4x4(&rc.projectionMatrix);
        BoundingFrustum::CreateFromMatrix(viewFrustum, proj);

        XMMATRIX invView = XMMatrixInverse(nullptr, XMLoadFloat4x4(&rc.viewMatrix));
        BoundingFrustum worldFrustum{};
        viewFrustum.Transform(worldFrustum, invView);
        return worldFrustum;
    }

    bool IsInstanceVisible(const DirectX::BoundingFrustum& worldFrustum, const ModelResource& modelResource, const InstanceData& instance)
    {
        using namespace DirectX;

        BoundingBox worldBounds{};
        const XMMATRIX world = XMLoadFloat4x4(&instance.worldMatrix);
        modelResource.GetLocalBounds().Transform(worldBounds, world);

        return worldFrustum.Contains(worldBounds) != DirectX::DISJOINT;
    }
}

void ExtractVisibleInstancesPass::Setup(FrameGraphBuilder& builder)
{
    (void)builder;
}

void ExtractVisibleInstancesPass::Execute(FrameGraphResources& resources, const RenderQueue& queue, RenderContext& rc)
{
    using Clock = std::chrono::high_resolution_clock;
    const auto startTime = Clock::now();

    (void)resources;
    rc.visibleOpaqueInstanceBatches.clear();
    rc.visibleOpaqueInstanceBatches.reserve(queue.opaqueInstanceBatches.size());
    rc.prepMetrics.visibleBatchCount = 0;
    rc.prepMetrics.visibleInstanceCount = 0;

    if (queue.opaqueInstanceBatches.empty()) {
        rc.prepMetrics.visibleExtractMs =
            std::chrono::duration<double, std::milli>(Clock::now() - startTime).count();
        return;
    }

    const auto worldFrustum = BuildWorldFrustum(rc);
    const size_t batchCount = queue.opaqueInstanceBatches.size();
    if (m_candidateBatches.capacity() < batchCount) {
        ++rc.prepMetrics.visibleScratchVectorGrowths;
    }
    if (m_nonEmptyFlags.capacity() < batchCount) {
        ++rc.prepMetrics.visibleScratchVectorGrowths;
    }
    m_candidateBatches.clear();
    m_candidateBatches.resize(batchCount);
    m_nonEmptyFlags.assign(batchCount, 0);

    TaskSystem::Instance().ParallelFor(
        batchCount,
        1,
        [&](size_t batchIndex) {
            const auto& batch = queue.opaqueInstanceBatches[batchIndex];
            if (!batch.modelResource) {
                return;
            }

            InstanceBatch visibleBatch{};
            visibleBatch.key = batch.key;
            visibleBatch.modelResource = batch.modelResource;
            visibleBatch.instances.reserve(batch.instances.size());

            for (const auto& instance : batch.instances) {
                if (IsInstanceVisible(worldFrustum, *batch.modelResource, instance)) {
                    visibleBatch.instances.push_back(instance);
                }
            }

            if (!visibleBatch.instances.empty()) {
                m_candidateBatches[batchIndex] = std::move(visibleBatch);
                m_nonEmptyFlags[batchIndex] = 1;
            }
        });

    uint32_t visibleInstanceCount = 0;
    for (size_t batchIndex = 0; batchIndex < m_candidateBatches.size(); ++batchIndex) {
        if (m_nonEmptyFlags[batchIndex] == 0) {
            continue;
        }

        visibleInstanceCount += static_cast<uint32_t>(m_candidateBatches[batchIndex].instances.size());
        rc.visibleOpaqueInstanceBatches.push_back(std::move(m_candidateBatches[batchIndex]));
    }

    rc.prepMetrics.visibleBatchCount = static_cast<uint32_t>(rc.visibleOpaqueInstanceBatches.size());
    rc.prepMetrics.visibleInstanceCount = visibleInstanceCount;
    rc.prepMetrics.visibleExtractMs =
        std::chrono::duration<double, std::milli>(Clock::now() - startTime).count();
}
