#pragma once
#include <vector>
#include <DirectXMath.h>
#include "Entity/Entity.h"

class Registry;

// Reads HealthComponent + HUDLinkComponent every frame and exposes the
// resolved values via a process-wide static snapshot. Render code reads
// the snapshot to drive the actual sprite-based HP bars (which are not
// yet wired into the render pipeline as of this commit; this system
// makes the data they will need available regardless).
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
