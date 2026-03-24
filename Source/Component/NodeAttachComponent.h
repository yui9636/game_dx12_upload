#pragma once
#include "Component/Component.h"
#include <unordered_map>
#include <string>
#include <vector>
#include <memory>
#include <DirectXMath.h>

class Model;
class Actor;

struct NodeSocket
{
    std::string name;
    std::string parentBoneName;

    DirectX::XMFLOAT3 offsetPos = { 0.0f, 0.0f, 0.0f };
    DirectX::XMFLOAT3 offsetRotDeg = { 0.0f, 0.0f, 0.0f }; // Euler Degrees
    DirectX::XMFLOAT3 offsetScale = { 1.0f, 1.0f, 1.0f };

    int cachedBoneIndex = -1;
};

class NodeAttachComponent : public Component
{
public:
    enum class OffsetSpace
    {
        NodeLocal,
        ModelLocal
    };

public:
    const char* GetName() const override { return "NodeAttach"; }

    void Start() override;
    void Update(float dt) override;
    void OnGUI() override;

    // ========================================================================
    // ========================================================================

    void RegisterSocket(const std::string& socketName, const std::string& boneName,
        const DirectX::XMFLOAT3& pos = { 0,0,0 },
        const DirectX::XMFLOAT3& rotDeg = { 0,0,0 },
        const DirectX::XMFLOAT3& scale = { 1,1,1 });

    void UnregisterSocket(const std::string& socketName);

    bool GetSocketWorldTransform(const std::string& socketName, DirectX::XMFLOAT4X4& outWorld);

    bool GetBoneWorldTransform(const std::string& boneName, DirectX::XMFLOAT4X4& outWorld);

    // ========================================================================
    // ========================================================================

    void BindTargetActor(std::shared_ptr<Actor> actor);

    void AttachSelfToSocket(const std::string& socketName);

    void AttachSelfToBone(const std::string& boneName);

    void Detach();

    bool IsAttached() const { return isAttached; }

    void SetOffset(const DirectX::XMFLOAT3& t) { defaultOffsetPos = t; }
    void SetEuler(const DirectX::XMFLOAT3& r) { defaultOffsetRot = r; }
    void SetScale(const DirectX::XMFLOAT3& s) { defaultOffsetScale = s; }
    void SetOffsetSpace(OffsetSpace s) { offsetSpace = s; }

    static DirectX::XMFLOAT3 GetWorldPosition_NodeLocal(
        const ::Model* model, int nodeIndex, const DirectX::XMFLOAT3& offsetLocal);

private:
    DirectX::XMFLOAT4X4 CalcWorldMatrix(int boneIndex, const ::Model* model, const DirectX::XMFLOAT4X4& actorWorld,
        const DirectX::XMFLOAT3& t, const DirectX::XMFLOAT3& r, const DirectX::XMFLOAT3& s) const;

    int ResolveBoneIndex(const ::Model* model, const std::string& name, int& cacheIndex);

    void UpdateBoneListCache(const ::Model* model);

private:
    std::shared_ptr<Actor> targetActor;
    const ::Model* lastModelPtr = nullptr;

    std::unordered_map<std::string, NodeSocket> sockets;

    bool isAttached = false;
    bool useSocketForSelf = false;
    std::string currentAttachName;

    DirectX::XMFLOAT3 defaultOffsetPos{ 0,0,0 };
    DirectX::XMFLOAT3 defaultOffsetRot{ 0,0,0 };
    DirectX::XMFLOAT3 defaultOffsetScale{ 1,1,1 };
    OffsetSpace       offsetSpace = OffsetSpace::NodeLocal;

    bool showDebugSockets = false;
    std::vector<std::string> guiBoneNameCache;
    int guiSelectedBoneIndex = 0;
};
