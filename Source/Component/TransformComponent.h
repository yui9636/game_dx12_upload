#pragma once
#include <DirectXMath.h>
#include "Entity/Entity.h"

struct TransformComponent {
    DirectX::XMFLOAT3 localPosition = { 0, 0, 0 };
    DirectX::XMFLOAT4 localRotation = { 0, 0, 0, 1 };
    DirectX::XMFLOAT3 localScale = { 1, 1, 1 };

    EntityID parent = 0;

    DirectX::XMFLOAT4X4 localMatrix;
    DirectX::XMFLOAT4X4 worldMatrix;
    DirectX::XMFLOAT4X4 prevWorldMatrix;

    DirectX::XMFLOAT3 worldPosition = { 0, 0, 0 };
    DirectX::XMFLOAT4 worldRotation = { 0, 0, 0, 1 };
    DirectX::XMFLOAT3 worldScale = { 1, 1, 1 };

    bool isDirty = true;
};
