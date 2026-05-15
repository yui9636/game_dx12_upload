// CameraBehaviorComponent はカメラ挙動の選択状態を保持する。
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
    float distance = 5.5f;
    float heightOffset = 2.2f;
    float lookAtHeight = 1.2f;
    float shoulderOffset = 0.0f;
    float forwardOffset = 0.0f;
    float lookAheadDistance = 0.0f;
    float smoothness = 0.0f;
    float rotationSmoothness = 0.0f;
    float pitch = 0.0f;
    float yaw = 0.0f;
    bool followTargetFacing = false;
    bool allowManualOrbit = false;
};

struct CameraLookAtComponent {
    EntityID target = Entity::NULL_ID;
    DirectX::XMFLOAT3 up = { 0, 1, 0 };
};
