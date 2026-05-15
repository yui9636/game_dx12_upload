#pragma once
#include <string>
#include <functional>
#include <memory>
#include "Terrain/TerrainAsset.h"

class Registry;

// Hierarchy 右クリック → "Create Terrain" で開くモーダルダイアログ。
// OnCreate コールバックに TerrainAsset が渡される。
class CreateTerrainDialog {
public:
    using CreateCallback = std::function<void(std::shared_ptr<TerrainAsset>)>;

    void Open(CreateCallback cb);
    void Draw(); // 毎フレーム呼ぶ (モーダル管理)

private:
    bool   m_open = false;
    char   m_name[128]     = "Terrain";
    int    m_resolution    = 512;
    float  m_worldSizeX    = kDefaultTerrainWorldSize;
    float  m_worldSizeZ    = kDefaultTerrainWorldSize;
    float  m_heightScale   = 64.0f;
    int    m_chunkCountX   = 8;
    int    m_chunkCountZ   = 8;

    CreateCallback m_callback;
};
