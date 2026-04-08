#pragma once

#include <string>
#include <DirectXMath.h>

struct NodeSocket
{
    std::string name;
    std::string parentBoneName;

    DirectX::XMFLOAT3 offsetPos = { 0.0f, 0.0f, 0.0f };
    DirectX::XMFLOAT3 offsetRotDeg = { 0.0f, 0.0f, 0.0f };
    DirectX::XMFLOAT3 offsetScale = { 1.0f, 1.0f, 1.0f };

    int cachedBoneIndex = -1;
};
