#pragma once
#include <vector>
#include <DirectXMath.h>
#include "Entity/Entity.h"

class Registry;

// HealthComponent と HUDLinkComponent を毎フレーム読み、
// 解決済みの値をプロセス内共有の静的スナップショットとして公開する。
// 描画側はこのスナップショットを読み、スプライトベースの HP バーを駆動する。
// 現時点では描画パイプラインへ完全接続されていないが、
// 必要なデータだけはこのシステムが先に用意する。
class HUDBindingSystem {
public:
    struct WorldEntry {
        EntityID entity = Entity::NULL_ID;
        DirectX::XMFLOAT3 worldPos{ 0.0f, 0.0f, 0.0f };
        float ratio = 1.0f;
        int hp = 0;
        int maxHP = 0;
        float worldOffsetY = 0.0f;
    };

    struct State {
        bool playerActive = false;
        EntityID playerEntity = Entity::NULL_ID;
        float playerRatio = 0.0f;
        int playerHP = 0;
        int playerMaxHP = 0;

        bool bossActive = false;
        EntityID bossEntity = Entity::NULL_ID;
        float bossRatio = 0.0f;
        int bossHP = 0;
        int bossMaxHP = 0;

        std::vector<WorldEntry> world;
    };

    static void Update(Registry& registry);
    static const State& GetState();
};
