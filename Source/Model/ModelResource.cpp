#include "ModelResource.h"

#include "Model.h"
#include "RHI/ICommandList.h"

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
}

void ModelResource::RebuildFromModel(const Model& model)
{
    const auto& meshes = model.GetMeshes();
    m_meshResources.clear();
    m_meshResources.reserve(meshes.size());

    for (const auto& mesh : meshes)
    {
        MeshResource resource{};
        resource.vertexBuffer = mesh.vertexBuffer;
        resource.indexBuffer = mesh.indexBuffer;
        resource.vertexStride = sizeof(Model::Vertex);
        resource.indexCount = static_cast<uint32_t>(mesh.indices.size());
        resource.materialIndex = mesh.materialIndex;
        resource.nodeIndex = mesh.nodeIndex;
        m_meshResources.push_back(std::move(resource));
    }

    SyncSceneDataFromModel(model);
}

void ModelResource::SyncSceneDataFromModel(const Model& model)
{
    const auto& meshes = model.GetMeshes();
    const auto& materials = model.GetMaterials();
    const auto& nodes = model.GetNodes();

    const auto getMaterial = [&](const Model::Mesh& mesh) -> Model::Material {
        if (mesh.materialIndex >= 0 && static_cast<size_t>(mesh.materialIndex) < materials.size()) {
            return materials[mesh.materialIndex];
        }
        if (mesh.material) {
            return *mesh.material;
        }
        return Model::Material{};
    };

    for (size_t meshIndex = 0; meshIndex < meshes.size() && meshIndex < m_meshResources.size(); ++meshIndex)
    {
        const auto& mesh = meshes[meshIndex];
        auto& resource = m_meshResources[meshIndex];

        resource.materialIndex = mesh.materialIndex;
        resource.nodeIndex = mesh.nodeIndex;
        resource.material = getMaterial(mesh);
        resource.nodeWorldTransform = IdentityMatrix();
        if (mesh.nodeIndex >= 0 && static_cast<size_t>(mesh.nodeIndex) < nodes.size()) {
            resource.nodeWorldTransform = nodes[mesh.nodeIndex].worldTransform;
        }

        resource.bones.clear();
        resource.bones.reserve(mesh.bones.size());
        for (const auto& bone : mesh.bones)
        {
            BoneResource boneResource{};
            boneResource.nodeIndex = bone.nodeIndex;
            boneResource.offsetTransform = bone.offsetTransform;
            boneResource.worldTransform = IdentityMatrix();
            if (bone.nodeIndex >= 0 && static_cast<size_t>(bone.nodeIndex) < nodes.size()) {
                boneResource.worldTransform = nodes[bone.nodeIndex].worldTransform;
            }
            resource.bones.push_back(std::move(boneResource));
        }
    }
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
