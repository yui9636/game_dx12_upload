#include "TerrainExtractSystem.h"
#include "TerrainComponent.h"
#include "TerrainBuildSystem.h"
#include "Registry/Registry.h"
#include "System/Query.h"
#include "RenderContext/RenderQueue.h"
#include "Component/TransformComponent.h"

void TerrainExtractSystem::Extract(
    Registry& registry,
    const TerrainBuildSystem& buildSys,
    RenderQueue& queue)
{
    Query<TerrainComponent> q(registry);
    q.ForEachWithEntity([&](EntityID entity, const TerrainComponent& tc) {
        if (!tc.showInEditor) return;
        const TerrainRuntimeData* rd = buildSys.GetRuntimeData(entity);
        if (!rd) return;

        for (const auto& chunk : rd->chunks) {
            if (!chunk.vertexBuffer || !chunk.indexBuffer) continue;

            TerrainChunkDrawCall dc;
            dc.vertexBuffer      = chunk.vertexBuffer.get();
            dc.indexBuffer       = chunk.indexBuffer.get();
            dc.indexCount        = chunk.indexCount;
            dc.chunkWorldOffset  = chunk.chunkWorldOffset;
            dc.worldSizeX        = rd->worldSizeX;
            dc.worldSizeZ        = rd->worldSizeZ;
            dc.heightScale       = rd->heightScale;
            queue.terrainChunks.push_back(dc);
        }
    });
}
