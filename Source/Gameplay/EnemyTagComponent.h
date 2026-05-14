#pragma once
#include <cstdint>

// AI 経路で処理される敵用タグ。PlayerTagComponent と対になる。
// PlayerTagComponent は入力デバイス経路用に予約されている。
// AI システム（BehaviorTreeSystem / PerceptionSystem）はこのタグを問い合わせる。
struct EnemyTagComponent
{
    uint16_t enemyKindId = 0; // 任意の分類 ID（knight=1, archer=2 など）。v1 では未使用。
};
