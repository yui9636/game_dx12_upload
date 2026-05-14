#pragma once

class Registry;

// プレイヤー側ロックオン。"LockOn" 入力 action の押下エッジで、
// 視野内で最も近い敵を取得して三人称カメラの target へ紐づけるか、
// ロックを解除してカメラをプレイヤーへ戻す。
// カメラが同じフレームで反応できるよう、TransformSystem より前に実行する。
class LockOnSystem {
public:
    static void Update(Registry& registry, float dt);
};
