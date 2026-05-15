#pragma once
#include <memory>
#include <vector>
#include <string>
#include <DirectXMath.h>
#include "Terrain/TerrainAsset.h"

// ECS コンポーネント。Terrain エンティティに付与する。
// GPU リソースはここには持たず、TerrainBuildSystem が管理する。
struct TerrainComponent {
    std::shared_ptr<TerrainAsset> asset;

    // ランタイムメッシュデータ(ビルド後に有効になる)
    bool needsRebuild = true;

    // エディタ用フラグ
    bool showInEditor = true;
};
