#pragma once
#include <cstdint>
// ActionNode は animIndex/nextLight/nextHeavy/nextDodge を保持し、関連システムが実行時状態として参照する。

struct ActionNode {
    int animIndex = 0;
    int nextLight = -1;
    int nextHeavy = -1;
    int nextDodge = -1;
    float inputStart = 0.0f;
    float inputEnd = 1.0f;
    float comboStart = 0.5f;
    float cancelStart = 0.2f;
    float magnetismRange = 10.0f;
    float magnetismSpeed = 0.0f;
    int damageVal = 0;
    float animSpeed = 1.0f;
};

struct ActionDatabaseComponent {
    static constexpr int MAX_NODES = 48;
    ActionNode nodes[MAX_NODES] = {};
    uint8_t nodeCount = 0;
};
