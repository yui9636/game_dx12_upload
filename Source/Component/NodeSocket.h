#pragma once

#include <string>
#include <DirectXMath.h>
// NodeSocket は name/parentBoneName/offsetPos を中心に、実行時やエディターで共有する状態を保持する。

struct NodeSocket
{
    std::string name;
    std::string parentBoneName;

    DirectX::XMFLOAT3 offsetPos = { 0.0f, 0.0f, 0.0f };
    DirectX::XMFLOAT3 offsetRotDeg = { 0.0f, 0.0f, 0.0f };
    DirectX::XMFLOAT3 offsetScale = { 1.0f, 1.0f, 1.0f };

    int cachedBoneIndex = -1;
};
