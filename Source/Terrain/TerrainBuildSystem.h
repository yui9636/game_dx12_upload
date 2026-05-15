#pragma once
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>
#include <DirectXMath.h>
#include "Entity/Entity.h"

class Registry;
class IBuffer;
class ITexture;

// TerrainComponent ごとの GPU チャンクバッファをまとめた構造体。
struct TerrainChunkBuffer {
    std::unique_ptr<IBuffer> vertexBuffer;
    std::unique_ptr<IBuffer> indexBuffer;
    uint32_t indexCount = 0;
    std::unique_ptr<IBuffer> lodIndexBuffers[5];
    uint32_t lodIndexCounts[5] = {};
    int currentLod = 0;
    DirectX::XMFLOAT3 chunkWorldOffset = { 0.0f, 0.0f, 0.0f };
    DirectX::XMFLOAT3 boundsCenter = { 0.0f, 0.0f, 0.0f };
    DirectX::XMFLOAT3 boundsExtents = { 0.0f, 0.0f, 0.0f };
};

struct TerrainRuntimeData {
    std::vector<TerrainChunkBuffer> chunks;
    float    worldSizeX  = 1280.0f;
    float    worldSizeZ  = 1280.0f;
    float    heightScale = 64.0f;
    uint32_t chunkCountX = 8;
    uint32_t chunkCountZ = 8;
    uint32_t resolution  = 512;
    // GPU テクスチャ (エンティティ単位で共有)
    std::unique_ptr<ITexture> gpuSplatMap;
    std::unique_ptr<ITexture> gpuAlbedos[3];
    std::unique_ptr<ITexture> gpuNormals[3];           // タンジェントスペース法線マップ
    std::unique_ptr<ITexture> gpuMRAs[3];              // R=Metallic, G=Roughness, B=AO
    std::string               loadedAlbedoPaths[3];   // 再ロード抑止用キャッシュ
    std::string               loadedNormalPaths[3];
    std::string               loadedMRAPaths[3];
    float                     layerTileScales[3] = { 8.0f, 6.0f, 4.0f };

    std::unique_ptr<IBuffer> waterVertexBuffer;
    std::unique_ptr<IBuffer> waterIndexBuffer;
    uint32_t waterIndexCount = 0;
    bool waterEnabled = false;
    float waterSeaLevel = 4.0f;
    DirectX::XMFLOAT4 waterShallowColor = { 0.12f, 0.44f, 0.56f, 0.55f };
    DirectX::XMFLOAT4 waterDeepColor = { 0.015f, 0.11f, 0.24f, 0.68f };
    float waterDepthFade = 5.0f;
    float waterWaveSpeed = 0.45f;
    float waterWaveScale = 0.035f;
};

// needsRebuild フラグが立っている TerrainComponent のメッシュを生成する。
class TerrainBuildSystem {
public:
    void Update(Registry& registry);

    // EntityID を指定してランタイムデータを取得 (ExtractSystem 用)。
    const TerrainRuntimeData* GetRuntimeData(EntityID entity) const;

    // 現在のカメラ位置からチャンクごとの LOD を更新する。
    void UpdateLod(Registry& registry,
                   const DirectX::XMFLOAT3& cameraPosition,
                   const float lodDistances[5]);

    // 全ランタイムデータを破棄する (シーン変更時)。
    void Clear();

private:
    void RebuildEntity(EntityID entity, struct TerrainAsset& asset);
    // スプラットテクスチャだけを差し替える軽量パス (ペイント用)。
    bool UploadSplatOnly(EntityID entity, struct TerrainAsset& asset);

    std::unordered_map<EntityID, TerrainRuntimeData> m_runtimeDataMap;
};
