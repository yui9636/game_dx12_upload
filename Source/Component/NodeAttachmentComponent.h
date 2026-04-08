#pragma once

#include <cstdint>
#include <string>

#include <DirectXMath.h>

#include "Entity/Entity.h"

enum class NodeAttachmentSpace : uint8_t
{
    NodeLocal = 0,
    ModelLocal = 1,
};

struct NodeAttachmentComponent
{
    EntityID targetEntity = Entity::NULL_ID;
    bool enabled = true;
    bool attached = false;
    bool useSocket = false;
    std::string attachName;
    DirectX::XMFLOAT3 offsetLocal = { 0.0f, 0.0f, 0.0f };
    DirectX::XMFLOAT3 offsetRotDeg = { 0.0f, 0.0f, 0.0f };
    DirectX::XMFLOAT3 offsetScale = { 1.0f, 1.0f, 1.0f };
    NodeAttachmentSpace offsetSpace = NodeAttachmentSpace::NodeLocal;
    int cachedBoneIndex = -1;
};
