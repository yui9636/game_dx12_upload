#include "Transform/NodeAttachmentUtils.h"

#include "Component/NodeSocket.h"
#include "Component/NodeSocketComponent.h"
#include "Model/Model.h"

using namespace DirectX;

namespace
{
    XMMATRIX BuildOffsetMatrix(const XMFLOAT3& offsetPos, const XMFLOAT3& offsetRotDeg, const XMFLOAT3& offsetScale)
    {
        const XMMATRIX scale = XMMatrixScaling(offsetScale.x, offsetScale.y, offsetScale.z);
        const XMMATRIX rotation = XMMatrixRotationRollPitchYaw(
            XMConvertToRadians(offsetRotDeg.x),
            XMConvertToRadians(offsetRotDeg.y),
            XMConvertToRadians(offsetRotDeg.z));
        const XMMATRIX translation = XMMatrixTranslation(offsetPos.x, offsetPos.y, offsetPos.z);
        return scale * rotation * translation;
    }
}

int NodeAttachmentUtils::ResolveBoneIndex(const Model* model, const std::string& name, int& cacheIndex)
{
    if (!model) {
        return -1;
    }

    const auto& nodes = model->GetNodes();
    if (cacheIndex >= 0 && cacheIndex < static_cast<int>(nodes.size()) && nodes[cacheIndex].name == name) {
        return cacheIndex;
    }

    for (int i = 0; i < static_cast<int>(nodes.size()); ++i) {
        if (nodes[i].name == name) {
            cacheIndex = i;
            return i;
        }
    }

    cacheIndex = -1;
    return -1;
}

XMFLOAT4X4 NodeAttachmentUtils::ComposeAttachmentWorldMatrix(
    const XMFLOAT4X4& baseWorld,
    const XMFLOAT3& offsetPos,
    const XMFLOAT3& offsetRotDeg,
    const XMFLOAT3& offsetScale,
    NodeAttachmentSpace offsetSpace)
{
    const XMMATRIX base = XMLoadFloat4x4(&baseWorld);
    const XMMATRIX offset = BuildOffsetMatrix(offsetPos, offsetRotDeg, offsetScale);

    XMMATRIX result = XMMatrixIdentity();
    if (offsetSpace == NodeAttachmentSpace::NodeLocal) {
        result = offset * base;
    }
    else {
        XMVECTOR outScale, outRot, outPos;
        XMMatrixDecompose(&outScale, &outRot, &outPos, base);
        const XMMATRIX translationOnly = XMMatrixTranslationFromVector(outPos);
        result = offset * translationOnly;
    }

    XMFLOAT4X4 world;
    XMStoreFloat4x4(&world, result);
    return world;
}

bool NodeAttachmentUtils::TryGetBoneWorldMatrix(
    const Model* model,
    const XMFLOAT4X4& targetWorld,
    const std::string& boneName,
    int& cacheIndex,
    const XMFLOAT3& offsetPos,
    const XMFLOAT3& offsetRotDeg,
    const XMFLOAT3& offsetScale,
    NodeAttachmentSpace offsetSpace,
    XMFLOAT4X4& outWorld)
{
    const int boneIndex = ResolveBoneIndex(model, boneName, cacheIndex);
    if (boneIndex < 0 || !model) {
        return false;
    }

    const auto& nodes = model->GetNodes();
    if (boneIndex >= static_cast<int>(nodes.size())) {
        return false;
    }

    const XMMATRIX boneWorld = XMLoadFloat4x4(&nodes[boneIndex].worldTransform);
    const XMMATRIX ownerWorld = XMLoadFloat4x4(&targetWorld);
    XMFLOAT4X4 baseWorld;
    XMStoreFloat4x4(&baseWorld, boneWorld * ownerWorld);

    outWorld = ComposeAttachmentWorldMatrix(baseWorld, offsetPos, offsetRotDeg, offsetScale, offsetSpace);
    return true;
}

const NodeSocket* NodeAttachmentUtils::FindSocket(const NodeSocketComponent* sockets, const std::string& socketName)
{
    if (!sockets) {
        return nullptr;
    }

    for (const auto& socket : sockets->sockets) {
        if (socket.name == socketName) {
            return &socket;
        }
    }

    return nullptr;
}

NodeSocket* NodeAttachmentUtils::FindSocket(NodeSocketComponent* sockets, const std::string& socketName)
{
    if (!sockets) {
        return nullptr;
    }

    for (auto& socket : sockets->sockets) {
        if (socket.name == socketName) {
            return &socket;
        }
    }

    return nullptr;
}

bool NodeAttachmentUtils::TryGetSocketWorldMatrix(
    const Model* model,
    const XMFLOAT4X4& targetWorld,
    NodeSocket& socket,
    XMFLOAT4X4& outWorld)
{
    return TryGetBoneWorldMatrix(
        model,
        targetWorld,
        socket.parentBoneName,
        socket.cachedBoneIndex,
        socket.offsetPos,
        socket.offsetRotDeg,
        socket.offsetScale,
        NodeAttachmentSpace::NodeLocal,
        outWorld);
}

bool NodeAttachmentUtils::TryResolveNamedAttachmentWorldMatrix(
    const Model* model,
    const XMFLOAT4X4& targetWorld,
    NodeSocketComponent* sockets,
    const std::string& attachName,
    bool preferSocket,
    int& cachedBoneIndex,
    const XMFLOAT3& offsetPos,
    const XMFLOAT3& offsetRotDeg,
    const XMFLOAT3& offsetScale,
    NodeAttachmentSpace offsetSpace,
    XMFLOAT4X4& outWorld)
{
    if (attachName.empty()) {
        return false;
    }

    if (preferSocket) {
        if (NodeSocket* socket = FindSocket(sockets, attachName)) {
            XMFLOAT4X4 socketWorld;
            if (TryGetSocketWorldMatrix(model, targetWorld, *socket, socketWorld)) {
                outWorld = ComposeAttachmentWorldMatrix(socketWorld, offsetPos, offsetRotDeg, offsetScale, offsetSpace);
                return true;
            }
        }
    }

    return TryGetBoneWorldMatrix(
        model,
        targetWorld,
        attachName,
        cachedBoneIndex,
        offsetPos,
        offsetRotDeg,
        offsetScale,
        offsetSpace,
        outWorld);
}

XMFLOAT3 NodeAttachmentUtils::GetWorldPositionNodeLocal(
    const Model* model,
    int nodeIndex,
    const XMFLOAT3& offsetLocal)
{
    if (!model) {
        return { 0.0f, 0.0f, 0.0f };
    }

    const auto& nodes = model->GetNodes();
    if (nodeIndex < 0 || nodeIndex >= static_cast<int>(nodes.size())) {
        return { 0.0f, 0.0f, 0.0f };
    }

    const XMMATRIX boneWorld = XMLoadFloat4x4(&nodes[nodeIndex].worldTransform);
    const XMMATRIX translation = XMMatrixTranslation(offsetLocal.x, offsetLocal.y, offsetLocal.z);
    XMFLOAT4X4 result;
    XMStoreFloat4x4(&result, translation * boneWorld);
    return { result._41, result._42, result._43 };
}
