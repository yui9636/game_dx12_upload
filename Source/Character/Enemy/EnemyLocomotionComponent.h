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


    void MoveTo(const DirectX::XMFLOAT3& targetPos);

    void Stop();

    void SetSpeed(float speed) { moveSpeed = speed; }

    void SetArenaRadius(float radius) { arenaRadius = radius; }

    bool IsMoving() const { return isMoving; }
    float GetCurrentSpeed() const { return currentSpeed; }

private:
    std::weak_ptr<Character> characterWk;

    DirectX::XMFLOAT3 targetPosition = { 0.0f, 0.0f, 0.0f };
    bool isMoving = false;

    float moveSpeed = 10.0f;
    float turnSpeed = 360.0f;
    float acceleration = 20.0f;
    float deceleration = 20.0f;

    float arenaRadius = 40.0f;
    float arrivalDistance = 1.0f;

    float currentSpeed = 0.0f;
};
