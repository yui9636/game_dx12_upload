#pragma once
#include <cstdint>

// Tag for AI-routed enemies. Parallel to PlayerTagComponent
// (which is reserved for input device routing).
// AI systems (BehaviorTreeSystem / PerceptionSystem) query for this tag.
struct EnemyTagComponent
{
    uint16_t enemyKindId = 0; // optional taxonomy id (knight=1, archer=2, ...). v1 unused.
};
