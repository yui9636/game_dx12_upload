#include "GrassExtractSystem.h"
#include "GrassComponent.h"
#include "GrassBuildSystem.h"
#include "Component/TransformComponent.h"
#include "Registry/Registry.h"
#include "System/Query.h"
#include "RenderContext/RenderQueue.h"

void GrassExtractSystem::Extract(Registry& registry, const GrassBuildSystem& buildSys, RenderQueue& queue)
{
    Query<GrassComponent> q(registry);
    q.ForEachWithEntity([&](EntityID entity, const GrassComponent& gc) {
        if (!gc.enabled || !gc.showInEditor) return;
        const GrassRuntimeData* rd = buildSys.GetRuntimeData(entity);
        if (!rd) return;

        DirectX::XMFLOAT3 entityOffset = { 0.0f, 0.0f, 0.0f };
        if (const TransformComponent* transform = registry.GetComponent<TransformComponent>(entity)) {
            entityOffset = transform->worldPosition;
        }

        // Emit one draw call per active layer.
        for (const FoliageLayerRuntime& lr : rd->layers) {
            if (!lr.meshVertexBuffer || !lr.meshIndexBuffer || !lr.instanceBuffer) continue;
            if (lr.instanceCount == 0) continue;

            GrassDrawCall dc;
            dc.meshVertexBuffer = lr.meshVertexBuffer;
            dc.meshIndexBuffer  = lr.meshIndexBuffer;
            dc.meshIndexCount   = lr.meshIndexCount;
            dc.meshVertexStride = lr.meshVertexStride;
            dc.albedoTexture    = lr.albedoTexture;
            dc.alphaCutoff      = lr.alphaCutoff;
            dc.instanceBuffer   = lr.instanceBuffer.get();
            dc.instanceCount    = lr.instanceCount;
            dc.boundsCenter     = {
                rd->boundsCenter.x + entityOffset.x,
                rd->boundsCenter.y + entityOffset.y,
                rd->boundsCenter.z + entityOffset.z
            };
            dc.boundsExtents    = rd->boundsExtents;
            dc.meshLocalMin     = lr.meshLocalMin;
            dc.meshLocalMax     = lr.meshLocalMax;
            dc.windDirection    = rd->windDirection;
            dc.windStrength     = lr.windStrength;
            dc.windSpeed        = lr.windSpeed;
            dc.colorBottom      = lr.colorBottom;
            dc.colorTop         = lr.colorTop;
            dc.drawDistance     = rd->drawDistance;
            dc.useWind          = lr.useWind;
            queue.grassDraws.push_back(dc);
        }
    });
}
