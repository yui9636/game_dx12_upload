#pragma once
#include "Component/Component.h"
#include <DirectXMath.h>
#include <memory>

class Character;
class AnimatorComponent;
class InputActionComponent;

class LocomotionComponent final : public Component
{
public:
    const char* GetName() const override { return "Locomotion"; }

    void Start() override;
    void Update(float dt) override;
    void OnGUI() override;

    // --- 外部からの制御 ---
    void SetMoveInput(const DirectX::XMFLOAT2& moveInput);
    void StopMovement();

    // --- 情報取得 ---
    int   GetGaitIndex() const { return currentGaitIndex; }
    int   GetDirectionIndex8() const { return directionIndex8; }
    float GetCurrentSpeed() const { return currentSpeed; }
    bool  IsTurningInPlace() const { return isTurningInPlace; }
    int   GetLastTurnSign() const { return lastTurnSign; }
    float GetLeanAngle() const { return currentLeanAngle; }

private:
    void  UpdateGait(float inputStrength);
    void  UpdatePhysics(float dt);
    void  UpdateRotation(float dt);
    void  UpdateDirectionIndex();
    void  SyncAnimationSpeed();

    static float Clamp01(float v);
    static float WrapAngle180(float deg);
    static int   AngleDegreeToIndex8(float deg);

private:
    std::weak_ptr<Character> characterWk;
    std::weak_ptr<AnimatorComponent> animatorWk;

    // --- 入力状態 ---
    DirectX::XMFLOAT2 inputVector{ 0.0f, 0.0f };
    DirectX::XMFLOAT2 moveDirection{ 0.0f, 1.0f };
    float             inputStrength = 0.0f;

    // --- パラメータ設定 (Physics) ---
    float walkMaxSpeed = 60.0f;
    float joggingMaxSpeed = 160.0f; // 少し上げました
    float runMaxSpeed = 200.0f;     // 疾走感を出すために上げました

    // ★重要: キビキビ動くための設定
    float acceleration = 60.0f;     // ベース加速度 (後半の伸び)
    float launchBoost = 4.0f;       // ★追加: 初速の爆発力 (4倍速で発進)
    float deceleration = 20.0f;   // ★超重要: ほぼ瞬時に止まる (2000なら慣性ゼロに近い)
    float turnSpeed = 1080.0f;      // 旋回も超高速に (3回転/秒)

    // 閾値
    float thresholdWalkEnter = 0.10f;
    float thresholdJogEnter = 0.35f;
    float thresholdRunEnter = 0.90f;

    // --- アニメーション同期用 ---
    float animDurationWalk = 1.07f;
    float animDurationJog = 0.93f;
    float animDurationRun = 0.53f;

    float animStrideWalk = 65.0f;
    float animStrideJog = 112.0f;
    float animStrideRun = 160.0f;

    // --- 実行時状態 ---
    int   currentGaitIndex = 0;
    float currentSpeed = 0.0f;

    // 8方向管理
    int   directionIndex8 = 0;
    int   lastValidDirectionIndex8 = 0;
    float holdHalfAngleDegree = 15.0f;
    float switchHalfAngleDegree = 30.0f;

    bool  isTurningInPlace = false;
    int   lastTurnSign = 0;

    float currentLeanAngle = 0.0f;
};