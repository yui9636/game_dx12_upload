#pragma once
#include <DirectXMath.h>

struct CharacterPhysicsComponent {
    DirectX::XMFLOAT3 velocity = { 0, 0, 0 };
    float verticalVelocity = 0.0f;
    float gravity = -30.0f;
    float friction = 5.0f;
    float maxMoveSpeed = 10.0f;
    float acceleration = 50.0f;
    bool isGround = true;
    float height = 1.8f;
    float stepOffset = 0.3f;
};
