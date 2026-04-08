#pragma once

#include <string>
#include <DirectXMath.h>
#include "Entity/Entity.h"

struct EffectAttachmentComponent
{
    EntityID parentEntity = Entity::NULL_ID;
    std::string socketName;
    DirectX::XMFLOAT3 offsetLocal = { 0.0f, 0.0f, 0.0f };
    DirectX::XMFLOAT3 offsetRotDeg = { 0.0f, 0.0f, 0.0f };
    DirectX::XMFLOAT3 offsetScale = { 1.0f, 1.0f, 1.0f };
};
