#include "GrassExtractSystem.h"
#include "GrassComponent.h"
#include "GrassBuildSystem.h"
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
        if (!rd->meshVertexBuffer || !rd->meshIndexBuffer || !rd->instanceBuffer) return;
        if (rd->instanceCount == 0) return;

        GrassDrawCall dc;
        dc.meshVertexBuffer = rd->meshVertexBuffer.get();
        dc.meshIndexBuffer  = rd->meshIndexBuffer.get();
        dc.meshIndexCount   = rd->meshIndexCount;
        dc.instanceBuffer   = rd->instanceBuffer.get();
        dc.instanceCount    = rd->instanceCount;
        dc.boundsCenter     = rd->boundsCenter;
        dc.boundsExtents    = rd->boundsExtents;
        dc.windDirection    = rd->windDirection;
        dc.windStrength     = rd->windStrength;
        dc.windSpeed        = rd->windSpeed;
        dc.colorBottom      = rd->colorBottom;
        dc.colorTop         = rd->colorTop;
        dc.drawDistance     = rd->drawDistance;
        queue.grassDraws.push_back(dc);
    });
}
