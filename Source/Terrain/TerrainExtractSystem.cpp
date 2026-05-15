#include "TerrainExtractSystem.h"
#include "TerrainComponent.h"
#include "TerrainBuildSystem.h"
#include "Registry/Registry.h"
#include "System/Query.h"
#include "RenderContext/RenderQueue.h"
#include "Component/TransformComponent.h"
#include <algorithm>

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

        DirectX::XMFLOAT3 entityOffset = { 0.0f, 0.0f, 0.0f };
        if (const TransformComponent* transform = registry.GetComponent<TransformComponent>(entity)) {
            entityOffset = transform->worldPosition;
        }

        for (const auto& chunk : rd->chunks) {
            const int lod = std::clamp(chunk.currentLod, 0, 4);
            IBuffer* indexBuffer = chunk.lodIndexBuffers[lod]
                ? chunk.lodIndexBuffers[lod].get()
                : chunk.indexBuffer.get();
            const uint32_t indexCount = chunk.lodIndexBuffers[lod]
                ? chunk.lodIndexCounts[lod]
                : chunk.indexCount;
            if (!chunk.vertexBuffer || !indexBuffer || indexCount == 0) continue;

            TerrainChunkDrawCall dc;
            dc.vertexBuffer      = chunk.vertexBuffer.get();
            dc.indexBuffer       = indexBuffer;
            dc.indexCount        = indexCount;
            dc.chunkWorldOffset  = {
                chunk.chunkWorldOffset.x + entityOffset.x,
                chunk.chunkWorldOffset.y + entityOffset.y,
                chunk.chunkWorldOffset.z + entityOffset.z
            };
            dc.boundsCenter = {
                chunk.boundsCenter.x + entityOffset.x,
                chunk.boundsCenter.y + entityOffset.y,
                chunk.boundsCenter.z + entityOffset.z
            };
            dc.boundsExtents = chunk.boundsExtents;
            dc.worldSizeX        = rd->worldSizeX;
            dc.worldSizeZ        = rd->worldSizeZ;
            dc.heightScale       = rd->heightScale;
            dc.splatTexture      = rd->gpuSplatMap.get();
            for (int i = 0; i < 3; ++i) {
                dc.albedoTextures[i]  = rd->gpuAlbedos[i].get();
                dc.normalTextures[i]  = rd->gpuNormals[i].get();
                dc.mraTextures[i]     = rd->gpuMRAs[i].get();
                dc.layerTileScales[i] = rd->layerTileScales[i];
            }
            queue.terrainChunks.push_back(dc);
        }

        if (rd->waterEnabled && rd->waterVertexBuffer && rd->waterIndexBuffer && rd->waterIndexCount > 0) {
            TerrainWaterDrawCall water;
            water.vertexBuffer = rd->waterVertexBuffer.get();
            water.indexBuffer = rd->waterIndexBuffer.get();
            water.indexCount = rd->waterIndexCount;
            water.worldOffset = entityOffset;
            water.worldSizeX = rd->worldSizeX;
            water.worldSizeZ = rd->worldSizeZ;
            water.seaLevel = rd->waterSeaLevel;
            water.shallowColor = rd->waterShallowColor;
            water.deepColor = rd->waterDeepColor;
            water.depthFade = rd->waterDepthFade;
            water.waveSpeed = rd->waterWaveSpeed;
            water.waveScale = rd->waterWaveScale;
            queue.terrainWater.push_back(water);
        }
    });
}
