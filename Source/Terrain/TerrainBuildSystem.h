#pragma once
#include <memory>
#include <unordered_map>
#include <vector>
#include <DirectXMath.h>
#include "Entity/Entity.h"

class Registry;
class IBuffer;

// TerrainComponent ごとの GPU チャンクバッファをまとめた構造体。
struct TerrainChunkBuffer {
    std::unique_ptr<IBuffer> vertexBuffer;
    std::unique_ptr<IBuffer> indexBuffer;
    uint32_t indexCount = 0;
    DirectX::XMFLOAT3 chunkWorldOffset = { 0.0f, 0.0f, 0.0f };
};

struct TerrainRuntimeData {
    std::vector<TerrainChunkBuffer> chunks;
    float    worldSizeX  = 512.0f;
    float    worldSizeZ  = 512.0f;
    float    heightScale = 64.0f;
    uint32_t chunkCountX = 8;
    uint32_t chunkCountZ = 8;
    uint32_t resolution  = 512;
};

// needsRebuild フラグが立っている TerrainComponent のメッシュを生成する。
class TerrainBuildSystem {
public:
    void Update(Registry& registry);

    // EntityID を指定してランタイムデータを取得 (ExtractSystem 用)。
    const TerrainRuntimeData* GetRuntimeData(EntityID entity) const;

    // 全ランタイムデータを破棄する (シーン変更時)。
    void Clear();

private:
    void RebuildEntity(EntityID entity, struct TerrainAsset& asset);

    std::unordered_map<EntityID, TerrainRuntimeData> m_runtimeDataMap;
};
