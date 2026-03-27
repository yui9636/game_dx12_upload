#pragma once

#include <cstdint>
#include <memory>
#include <vector>
#include <DirectXCollision.h>
#include "Model.h"

class IBuffer;
class ICommandList;
class IResourceFactory;

class ModelResource
{
public:
    struct BoneResource
    {
        int nodeIndex = -1;
        DirectX::XMFLOAT4X4 offsetTransform = {};
        DirectX::XMFLOAT4X4 worldTransform = {};
    };

    struct MeshResource
    {
        std::shared_ptr<IBuffer> vertexBuffer;
        std::shared_ptr<IBuffer> indexBuffer;
        uint32_t vertexStride = 0;
        uint32_t indexCount = 0;
        int materialIndex = -1;
        int nodeIndex = -1;
        Model::Material material;
        DirectX::XMFLOAT4X4 nodeWorldTransform = {};
        DirectX::BoundingBox localBounds = {};
        std::vector<BoneResource> bones;
    };

    ModelResource() = default;
    ~ModelResource() = default;

    void RebuildFromModel(const Model& model, IResourceFactory* factory);
    void SyncSceneDataFromModel(const Model& model);
    void SyncMeshBuffers(int meshIndex,
        const std::shared_ptr<IBuffer>& vertexBuffer,
        const std::shared_ptr<IBuffer>& indexBuffer,
        uint32_t vertexStride,
        uint32_t indexCount,
        int materialIndex,
        int nodeIndex);

    const MeshResource* GetMeshResource(int meshIndex) const;
    MeshResource* GetMeshResource(int meshIndex);
    bool BindMeshBuffers(ICommandList* commandList, int meshIndex) const;
    uint32_t GetMeshIndexCount(int meshIndex) const;
    int GetMeshCount() const { return static_cast<int>(m_meshResources.size()); }

    const std::vector<MeshResource>& GetMeshResources() const { return m_meshResources; }
    const DirectX::BoundingBox& GetLocalBounds() const { return m_localBounds; }
    bool HasSkinnedMeshes() const { return m_hasSkinnedMeshes; }

private:
    std::vector<MeshResource> m_meshResources;
    DirectX::BoundingBox m_localBounds = {};
    bool m_hasSkinnedMeshes = false;
};
