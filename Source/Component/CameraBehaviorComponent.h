#pragma once
#include "Entity/Entity.h"
#include <DirectXMath.h>

struct CameraFreeControlComponent {
    float moveSpeed = 20.0f;
    float rotateSpeed = 0.005f;
    float pitch = 0.0f;
    float yaw = 0.0f;

    bool isHovered = false;
};

struct CameraTPVControlComponent {
    EntityID target = Entity::NULL_ID;
    float distance = 5.0f;
    float heightOffset = 1.5f;
    float smoothness = 8.0f;
    float pitch = 0.0f;
    float yaw = 0.0f;
};

struct CameraLookAtComponent {
    EntityID target = Entity::NULL_ID;
    DirectX::XMFLOAT3 up = { 0, 1, 0 };
};
