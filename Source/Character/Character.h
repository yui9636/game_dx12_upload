#pragma once
#include <DirectXMath.h>
#include <SimpleMath.h>

using namespace DirectX::SimpleMath;

#include "Actor/Actor.h"

class Character : public Actor
{
public:
    Character() = default;
    virtual ~Character() = default;

    virtual void Start() override;

    void UpdateTransform();

    void Render(ModelRenderer* renderer) override;
 
    const DirectX::XMFLOAT3& GetPosition() const { return Actor::GetPosition(); }

    void SetPosition(const DirectX::XMFLOAT3& position) {
        this->position = position;
        Actor::SetPosition(position);
    }

    const DirectX::XMFLOAT3& GetScale() const { return Actor::GetScale(); }

    void SetScale(const DirectX::XMFLOAT3& scale) {
        this->scale = scale;
        Actor::SetScale(scale);
    }

    const DirectX::XMFLOAT3& GetAngle() const { return angle; }
    void SetAngle(const DirectX::XMFLOAT3& angle) { this->angle = angle; }

    float GetRadius() const { return radius; }

    bool IsGround() const { return isGround; }

    float GetHeight() const { return height; }

    bool ApplyDamage(int damage, float invincibleTime);

    void AddImpulse(const DirectX::XMFLOAT3& impulse);

    int GetHealth() const { return health; }

    int GetMaxHealth() const { return maxHealth; }

    void Move(float vx, float vz, float speed);

    void Turn(float dt, float vx, float vz, float speed);

    void ApplyStageConstraint(float stageRadius);
protected:


    void Jump(float speed);

    void UpdateVelocity(float dt);

    void UpdateInvincibleTimer(float dt);

    virtual void OnLanding() {}

    virtual void OnDamaged() {}

    virtual void OnDead() {}

    void UpdateHorizontalMove(float dt);
private:
    void UpdateVerticalVelocity(float elapsedFrame);

    void UpdateVerticalMove(float dt);

    void UpdateHorizontalVelocity(float elapsedFrame);

public:
    DirectX::XMFLOAT3   position = { 0,0,0 };
    DirectX::XMFLOAT3   angle = { 0,0,0 };
    DirectX::XMFLOAT3   scale = { 1,1,1 };
    DirectX::XMFLOAT4X4 transform = {
        1,0,0,0,
        0,1,0,0,
        0,0,1,0,
        0,0,0,1
    };

    float radius = 0.5f;

    float gravity = -1.0f;

    DirectX::XMFLOAT3 velocity = { 0,0,0 };

    bool isGround = false;

    float height = 2.0f;

    int health = 5;

    // MAXHP
    int maxHealth = 5;

    float invincibleTimer = 1.0f;

    float friction = 10.8f;

    float acceleration = 50.0f;

    float maxMoveSpeed = 100.0f;

    float moveVecX = 0.0f;
    float moveVecZ = 0.0f;

    float airControl = 0.3f;

    float stepOffset = 1.0f;

    float slopeRate = 1.0f;

    bool isInvincible = false;

    int lastDamage = 0;
};
