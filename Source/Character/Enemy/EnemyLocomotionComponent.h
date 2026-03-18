#pragma once
#include "Component/Component.h"
#include <DirectXMath.h>
#include <memory>

class Character;

class EnemyLocomotionComponent final : public Component
{
public:
    const char* GetName() const override { return "EnemyLocomotion"; }

    void Start() override;
    void Update(float dt) override;
    void OnGUI() override;

    // --- AI (BehaviorTree) からの命令用 ---

    /// @brief 指定した座標へ向かって移動する
    /// @param targetPos 目標地点（Y座標は無視されます）
    void MoveTo(const DirectX::XMFLOAT3& targetPos);

    /// @brief 移動を停止する
    void Stop();

    /// @brief 移動速度を設定する
    void SetSpeed(float speed) { moveSpeed = speed; }

    /// @brief ステージの半径を設定（これより外には出られなくなる）
    void SetArenaRadius(float radius) { arenaRadius = radius; }

    // --- 状態取得 ---
    bool IsMoving() const { return isMoving; }
    float GetCurrentSpeed() const { return currentSpeed; }

private:
    std::weak_ptr<Character> characterWk;

    // 目標
    DirectX::XMFLOAT3 targetPosition = { 0.0f, 0.0f, 0.0f };
    bool isMoving = false;

    // パラメータ
    float moveSpeed = 10.0f;       // 標準移動速度
    float turnSpeed = 360.0f;     // 旋回速度 (deg/sec)
    float acceleration = 20.0f;   // 加速度
    float deceleration = 20.0f;   // 減速度

    // ★最重要: ステージ設定
    float arenaRadius = 40.0f;    // 円形ステージの半径
    float arrivalDistance = 1.0f; // 目標に到達したとみなす距離

    // 内部変数
    float currentSpeed = 0.0f;
};