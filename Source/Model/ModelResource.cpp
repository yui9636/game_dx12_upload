#include "ModelResource.h"

#include "Model.h"
#include "RHI/ICommandList.h"
#include "RHI/IResourceFactory.h"
#include "RHI/IBuffer.h"
#include <DirectXMath.h>

namespace
{
    DirectX::XMFLOAT4X4 IdentityMatrix()
    {
        return DirectX::XMFLOAT4X4(
            1.0f, 0.0f, 0.0f, 0.0f,
            0.0f, 1.0f, 0.0f, 0.0f,
            0.0f, 0.0f, 1.0f, 0.0f,
            0.0f, 0.0f, 0.0f, 1.0f);
    }

    DirectX::BoundingBox BuildLocalBounds(
        const Model& model,
        const std::vector<ModelResource::MeshResource>& meshResources)
    {
        using namespace DirectX;

        bool hasBounds = false;
        BoundingBox bounds{};
        const auto& nodes = model.GetNodes();

        auto IsFiniteMatrix = [](const XMFLOAT4X4& m) {
            const float* f = &m._11;
            for (int i = 0; i < 16; ++i) {
                if (!std::isfinite(f[i])) {
                    return false;
                }
            }
            return true;
            };

        for (size_t meshIndex = 0; meshIndex < meshResources.size(); ++meshIndex)
        {
            const auto& meshResource = meshResources[meshIndex];

            if (meshResource.localBounds.Extents.x == 0.0f &&
                meshResource.localBounds.Extents.y == 0.0f &&
                meshResource.localBounds.Extents.z == 0.0f)
            {
                continue;
            }

            XMMATRIX nodeWorld = XMMatrixIdentity();
            if (meshResource.nodeIndex >= 0 &&
                static_cast<size_t>(meshResource.nodeIndex) < nodes.size())
            {
                if (!IsFiniteMatrix(meshResource.nodeWorldTransform)) {
                    __debugbreak();
                    continue;
                }

                nodeWorld = XMLoadFloat4x4(&meshResource.nodeWorldTransform);
            }

            BoundingBox transformedBounds{};
            meshResource.localBounds.Transform(transformedBounds, nodeWorld);

            if (!std::isfinite(transformedBounds.Center.x) ||
                !std::isfinite(transformedBounds.Center.y) ||
                !std::isfinite(transformedBounds.Center.z) ||
                !std::isfinite(transformedBounds.Extents.x) ||
                !std::isfinite(transformedBounds.Extents.y) ||
                !std::isfinite(transformedBounds.Extents.z))
            {
                __debugbreak();
                continue;
            }

            if (!hasBounds) {
                bounds = transformedBounds;
                hasBounds = true;
            }
            else {
                BoundingBox merged{};
                BoundingBox::CreateMerged(merged, bounds, transformedBounds);
                bounds = merged;
            }
        }

        if (!hasBounds) {
            bounds.Center = { 0.0f, 0.0f, 0.0f };
            bounds.Extents = { 0.0f, 0.0f, 0.0f };
        }

        return bounds;
    }

    DirectX::BoundingBox BuildMeshLocalBounds(const Model::Mesh& mesh)
    {
        using namespace DirectX;

        BoundingBox bounds{};
        if (mesh.vertices.empty()) {
            bounds.Center = { 0.0f, 0.0f, 0.0f };
            bounds.Extents = { 0.0f, 0.0f, 0.0f };
            return bounds;
        }

        std::vector<XMFLOAT3> points;
        points.reserve(mesh.vertices.size());
        for (const auto& vertex : mesh.vertices) {
            points.push_back(vertex.position);
        }

        BoundingBox::CreateFromPoints(bounds, points.size(), points.data(), sizeof(XMFLOAT3));
        return bounds;
    }
}

void ModelResource::RebuildFromModel(const Model& model, IResourceFactory* factory)
{
    const auto& meshes = model.GetMeshes();
    m_meshResources.clear();
    m_meshResources.reserve(meshes.size());

    for (size_t meshIndex = 0; meshIndex < meshes.size(); ++meshIndex)
    {
        const auto& mesh = meshes[meshIndex];
        MeshResource resource{};
        resource.vertexStride = sizeof(Model::Vertex);
        resource.indexCount = static_cast<uint32_t>(mesh.indices.size());
        if (factory)
        {
            resource.vertexBuffer = std::shared_ptr<IBuffer>(
                factory->CreateBuffer(
                    static_cast<uint32_t>(sizeof(Model::Vertex) * mesh.vertices.size()),
                    BufferType::Vertex,
                    mesh.vertices.empty() ? nullptr : mesh.vertices.data()).release());

            resource.indexBuffer = std::shared_ptr<IBuffer>(
                factory->CreateBuffer(
                    static_cast<uint32_t>(sizeof(uint32_t) * mesh.indices.size()),
                    BufferType::Index,
                    mesh.indices.empty() ? nullptr : mesh.indices.data()).release());
        }
        resource.materialIndex = model.GetMeshMaterialIndex(static_cast<int>(meshIndex));
        resource.nodeIndex = model.GetMeshNodeIndex(static_cast<int>(meshIndex));
        resource.localBounds = BuildMeshLocalBounds(mesh);
        m_meshResources.push_back(std::move(resource));
    }

    SyncSceneDataFromModel(model);
    m_localBounds = BuildLocalBounds(model, m_meshResources);
}

void ModelResource::SyncSceneDataFromModel(const Model& model)
{
    const auto& meshes = model.GetMeshes();
    const auto& materials = model.GetMaterials();
    const auto& nodes = model.GetNodes();

    const auto getMaterial = [&](int meshIndex) -> Model::Material {
        const int materialIndex = model.GetMeshMaterialIndex(meshIndex);
        if (materialIndex >= 0 && static_cast<size_t>(materialIndex) < materials.size()) {
            return materials[materialIndex];
        }
        return Model::Material{};
    };

    for (size_t meshIndex = 0; meshIndex < meshes.size() && meshIndex < m_meshResources.size(); ++meshIndex)
    {
        const auto& mesh = meshes[meshIndex];
        auto& resource = m_meshResources[meshIndex];

        resource.materialIndex = model.GetMeshMaterialIndex(static_cast<int>(meshIndex));
        resource.nodeIndex = model.GetMeshNodeIndex(static_cast<int>(meshIndex));
        resource.material = getMaterial(static_cast<int>(meshIndex));
        resource.nodeWorldTransform = IdentityMatrix();
        if (resource.nodeIndex >= 0 && static_cast<size_t>(resource.nodeIndex) < nodes.size()) {
            resource.nodeWorldTransform = nodes[resource.nodeIndex].worldTransform;
        }

        resource.bones.clear();
        resource.bones.reserve(mesh.bones.size());
        for (size_t boneIndex = 0; boneIndex < mesh.bones.size(); ++boneIndex)
        {
            const auto& bone = mesh.bones[boneIndex];
            BoneResource boneResource{};
            boneResource.nodeIndex = model.GetMeshBoneNodeIndex(static_cast<int>(meshIndex), static_cast<int>(boneIndex));
            boneResource.offsetTransform = bone.offsetTransform;
            boneResource.worldTransform = IdentityMatrix();
            if (boneResource.nodeIndex >= 0 && static_cast<size_t>(boneResource.nodeIndex) < nodes.size()) {
                boneResource.worldTransform = nodes[boneResource.nodeIndex].worldTransform;
            }
            resource.bones.push_back(std::move(boneResource));
        }
    }

    m_localBounds = BuildLocalBounds(model, m_meshResources);
}

void ModelResource::SyncMeshBuffers(int meshIndex,
    const std::shared_ptr<IBuffer>& vertexBuffer,
    const std::shared_ptr<IBuffer>& indexBuffer,
    uint32_t vertexStride,
    uint32_t indexCount,
    int materialIndex,
    int nodeIndex)
{
    if (meshIndex < 0) return;
    if (static_cast<size_t>(meshIndex) >= m_meshResources.size()) {
        m_meshResources.resize(static_cast<size_t>(meshIndex) + 1);
    }

    MeshResource& resource = m_meshResources[meshIndex];
    resource.vertexBuffer = vertexBuffer;
    resource.indexBuffer = indexBuffer;
    resource.vertexStride = vertexStride;
    resource.indexCount = indexCount;
    resource.materialIndex = materialIndex;
    resource.nodeIndex = nodeIndex;
}

const ModelResource::MeshResource* ModelResource::GetMeshResource(int meshIndex) const
{
    if (meshIndex < 0 || static_cast<size_t>(meshIndex) >= m_meshResources.size()) {
        return nullptr;
    }
    return &m_meshResources[meshIndex];
}

ModelResource::MeshResource* ModelResource::GetMeshResource(int meshIndex)
{
    if (meshIndex < 0 || static_cast<size_t>(meshIndex) >= m_meshResources.size()) {
        return nullptr;
    }
    return &m_meshResources[meshIndex];
}

bool ModelResource::BindMeshBuffers(ICommandList* commandList, int meshIndex) const
{
    const MeshResource* meshResource = GetMeshResource(meshIndex);
    if (!commandList || !meshResource || !meshResource->vertexBuffer || !meshResource->indexBuffer) {
        return false;
    }

    commandList->SetVertexBuffer(0, meshResource->vertexBuffer.get(), meshResource->vertexStride, 0);
    commandList->SetIndexBuffer(meshResource->indexBuffer.get(), IndexFormat::Uint32, 0);
    return true;
}

uint32_t ModelResource::GetMeshIndexCount(int meshIndex) const
{
    const MeshResource* meshResource = GetMeshResource(meshIndex);
    return meshResource ? meshResource->indexCount : 0;
}
