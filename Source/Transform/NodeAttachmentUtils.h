#pragma once

#include <string>

#include <DirectXMath.h>

#include "Component/NodeAttachmentComponent.h"

class Model;
struct NodeSocket;
struct NodeSocketComponent;

namespace NodeAttachmentUtils
{
    int ResolveBoneIndex(const Model* model, const std::string& name, int& cacheIndex);

    DirectX::XMFLOAT4X4 ComposeAttachmentWorldMatrix(
        const DirectX::XMFLOAT4X4& baseWorld,
        const DirectX::XMFLOAT3& offsetPos,
        const DirectX::XMFLOAT3& offsetRotDeg,
        const DirectX::XMFLOAT3& offsetScale,
        NodeAttachmentSpace offsetSpace);

    bool TryGetBoneWorldMatrix(
        const Model* model,
        const DirectX::XMFLOAT4X4& targetWorld,
        const std::string& boneName,
        int& cacheIndex,
        const DirectX::XMFLOAT3& offsetPos,
        const DirectX::XMFLOAT3& offsetRotDeg,
        const DirectX::XMFLOAT3& offsetScale,
        NodeAttachmentSpace offsetSpace,
        DirectX::XMFLOAT4X4& outWorld);

    const NodeSocket* FindSocket(const NodeSocketComponent* sockets, const std::string& socketName);
    NodeSocket* FindSocket(NodeSocketComponent* sockets, const std::string& socketName);

    bool TryGetSocketWorldMatrix(
        const Model* model,
        const DirectX::XMFLOAT4X4& targetWorld,
        NodeSocket& socket,
        DirectX::XMFLOAT4X4& outWorld);

    bool TryResolveNamedAttachmentWorldMatrix(
        const Model* model,
        const DirectX::XMFLOAT4X4& targetWorld,
        NodeSocketComponent* sockets,
        const std::string& attachName,
        bool preferSocket,
        int& cachedBoneIndex,
        const DirectX::XMFLOAT3& offsetPos,
        const DirectX::XMFLOAT3& offsetRotDeg,
        const DirectX::XMFLOAT3& offsetScale,
        NodeAttachmentSpace offsetSpace,
        DirectX::XMFLOAT4X4& outWorld);

    DirectX::XMFLOAT3 GetWorldPositionNodeLocal(
        const Model* model,
        int nodeIndex,
        const DirectX::XMFLOAT3& offsetLocal);
}
