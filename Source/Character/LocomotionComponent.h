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

    void SetMoveInput(const DirectX::XMFLOAT2& moveInput);
    void StopMovement();

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

    DirectX::XMFLOAT2 inputVector{ 0.0f, 0.0f };
    DirectX::XMFLOAT2 moveDirection{ 0.0f, 1.0f };
    float             inputStrength = 0.0f;

    float walkMaxSpeed = 60.0f;
    float joggingMaxSpeed = 160.0f;
    float runMaxSpeed = 200.0f;

    float acceleration = 60.0f;
    float launchBoost = 4.0f;
    float deceleration = 20.0f;
    float turnSpeed = 1080.0f;

    // 臒l
    float thresholdWalkEnter = 0.10f;
    float thresholdJogEnter = 0.35f;
    float thresholdRunEnter = 0.90f;

    float animDurationWalk = 1.07f;
    float animDurationJog = 0.93f;
    float animDurationRun = 0.53f;

    float animStrideWalk = 65.0f;
    float animStrideJog = 112.0f;
    float animStrideRun = 160.0f;

    int   currentGaitIndex = 0;
    float currentSpeed = 0.0f;

    int   directionIndex8 = 0;
    int   lastValidDirectionIndex8 = 0;
    float holdHalfAngleDegree = 15.0f;
    float switchHalfAngleDegree = 30.0f;

    bool  isTurningInPlace = false;
    int   lastTurnSign = 0;

    float currentLeanAngle = 0.0f;
};
