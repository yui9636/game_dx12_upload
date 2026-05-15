#pragma once

class Registry;
class TerrainBuildSystem;
class RenderQueue;

// TerrainComponent を走査し、RenderQueue::terrainChunks に描画リクエストを積む。
class TerrainExtractSystem {
public:
    void Extract(Registry& registry, const TerrainBuildSystem& buildSys, RenderQueue& queue);
};
