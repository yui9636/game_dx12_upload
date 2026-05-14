#pragma once
#include <DirectXMath.h>

struct LocomotionStateComponent {
    DirectX::XMFLOAT2 moveInput = { 0, 0 };   // 生のスティック入力
    float inputStrength = 0.0f;                 // 入力強度 [0,1]
    DirectX::XMFLOAT2 worldMoveDir = { 0, 0 }; // カメラ相対のワールド方向（正規化済み）

    uint8_t gaitIndex = 0;       // 0 は Idle、1 は Walk、2 は Jog、3 は Run。
    float currentSpeed = 0.0f;   // 実際の移動速度

    float targetAngleY = 0.0f;   // 目標向き角（ラジアン）
    bool turningInPlace = false;
    int lastTurnSign = 0;        // +1 右、-1 左

    // 歩容しきい値（入力強度）
    float walkThreshold = 0.10f;
    float jogThreshold = 0.35f;
    float runThreshold = 0.90f;

    // 歩容ごとの最大速度
    float walkMaxSpeed = 1.6f;
    float jogMaxSpeed = 3.2f;
    float runMaxSpeed = 5.8f;

    // 物理
    float acceleration = 12.0f;
    float launchBoost = 1.0f;       // 最大速度 30% 未満で使う倍率
    float deceleration = 18.0f;
    float turnSpeed = 720.0f;       // 度/秒

    // moveInput の解釈。
    //   true  : カメラ相対のスティック入力（Player 既定）。
    //   false : ワールド空間 XZ 方向（AI / scripted movement）。
    // false の場合、LocomotionSystem はカメラ基底変換を省く。
    // EnemyAI_BehaviorTree_Spec_v1.0_2026-04-27.md の 3.7 / 5.5 章を参照。
    bool useCameraRelativeInput = true;
};
