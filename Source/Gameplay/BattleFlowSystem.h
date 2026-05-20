#pragma once

#include <string>

class FlowEventQueue;
class Registry;

// 実行時サービスを通じて 1 対 1 エンカウントのステートマシンを駆動する。
// GameFlow action から開始 / リセットされるため、
// Hierarchy 上の singleton entity は不要。
// フェーズ:
//   Idle      -> プレイヤーがアリーナ半径に入ると Encounter
//   Encounter -> introDuration 経過後に Combat
//   Combat    -> ボス死亡で Victory、プレイヤー死亡で Defeat
//   Victory/Defeat -> 外部から phase がリセットされるまで維持
class BattleFlowSystem {
public:
    static void Update(Registry& registry, float dt);
    static void Start(const std::string& battleId = std::string{});
    static void Reset();
    static void DrainEvents(FlowEventQueue& outEvents);

    // Combat time remaining in seconds while a time limit is active; -1 if none.
    static float GetRemainingTime();
    // Current phase as a BattleFlowComponent::Phase value (0=Idle .. 5=Draw).
    static int GetPhase();
};
