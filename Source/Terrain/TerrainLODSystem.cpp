#include "TerrainLODSystem.h"
#include "TerrainComponent.h"
#include "Registry/Registry.h"
#include "System/Query.h"
#include "Component/TransformComponent.h"
#include "Component/CameraComponent.h"

void TerrainLODSystem::Update(Registry& registry)
{
    // カメラ位置を取得する。
    DirectX::XMFLOAT3 cameraPos = { 0.0f, 0.0f, 0.0f };
    {
        Query<CameraMainTagComponent, TransformComponent> camQ(registry);
        camQ.ForEach([&](const CameraMainTagComponent&, const TransformComponent& t) {
            cameraPos = t.worldPosition;
        });
    }

    // テラインが存在すれば LOD は常に 0 (将来拡張)。
    Query<TerrainComponent> q(registry);
    q.ForEach([&](TerrainComponent& /*tc*/) {
        // Phase 2 以降: チャンク距離計算と LOD レベル設定を実装する。
    });
}
