#include "ExtractVisibleInstancesPass.h"
#include "Model/ModelResource.h"
#include <DirectXCollision.h>
#include <DirectXMath.h>

namespace
{
    bool IsInstanceVisible(const RenderContext& rc, const ModelResource& modelResource, const InstanceData& instance)
    {
        using namespace DirectX;

        BoundingFrustum viewFrustum{};
        const XMMATRIX proj = XMLoadFloat4x4(&rc.projectionMatrix);
        BoundingFrustum::CreateFromMatrix(viewFrustum, proj);

        XMMATRIX invView = XMMatrixInverse(nullptr, XMLoadFloat4x4(&rc.viewMatrix));
        BoundingFrustum worldFrustum{};
        viewFrustum.Transform(worldFrustum, invView);

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
    (void)resources;
    rc.visibleOpaqueInstanceBatches.clear();
    rc.visibleOpaqueInstanceBatches.reserve(queue.opaqueInstanceBatches.size());

    for (const auto& batch : queue.opaqueInstanceBatches)
    {
        if (!batch.modelResource) {
            continue;
        }

        InstanceBatch visibleBatch{};
        visibleBatch.key = batch.key;
        visibleBatch.modelResource = batch.modelResource;
        visibleBatch.instances.reserve(batch.instances.size());

        for (const auto& instance : batch.instances)
        {
            if (IsInstanceVisible(rc, *batch.modelResource, instance)) {
                visibleBatch.instances.push_back(instance);
            }
        }

        if (!visibleBatch.instances.empty()) {
            rc.visibleOpaqueInstanceBatches.push_back(std::move(visibleBatch));
        }
    }
}
