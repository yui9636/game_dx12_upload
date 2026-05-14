#pragma once

class Registry;

// CollisionManager のフレーム単位接触一覧を読み、
// Attack vs Body のヒットを DamageEventRuntimeQueue の record へ変換する。
// HealthSystem は同じフレームでそれらの event を消費し、queue をクリアする。
// GameLayer での実行順:
// TimelineHitboxSystem、CollisionSystem、DamageSystem、HealthSystem の順に被弾処理を流す。
class DamageSystem {
public:
    static void Update(Registry& registry);
};
