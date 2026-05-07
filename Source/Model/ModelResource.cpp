// Model から描画用 GPU リソースを構築・同期する実装です。
#include "ModelResource.h"

#include "Model.h"
#include "RHI/ICommandList.h"
#include "RHI/IResourceFactory.h"
#include "RHI/IBuffer.h"
#include <DirectXMath.h>

namespace
{
    // 描画側で未設定の transform を扱うときの安全な初期値。
    DirectX::XMFLOAT4X4 IdentityMatrix()
    {
        return DirectX::XMFLOAT4X4(
            1.0f, 0.0f, 0.0f, 0.0f,
            0.0f, 1.0f, 0.0f, 0.0f,
            0.0f, 0.0f, 1.0f, 0.0f,
            0.0f, 0.0f, 0.0f, 1.0f);
    }

    // 各メッシュのローカル境界を node の現在 transform で合成し、描画用のモデル境界を作る。
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

    // 1 メッシュ分の頂点からローカル境界を計算します。
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

// Model のメッシュ情報から GPU バッファと描画用メッシュ情報を再構築します。
void ModelResource::RebuildFromModel(const Model& model, IResourceFactory* factory)
{
    // CPU 側 Model から GPU バッファと描画メタデータを作り直す。
    const auto& meshes = model.GetMeshes();
    m_meshResources.clear();
    m_meshResources.reserve(meshes.size());
    m_hasSkinnedMeshes = false;

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
        if (!mesh.bones.empty()) {
            m_hasSkinnedMeshes = true;
        }
        m_meshResources.push_back(std::move(resource));
    }

    SyncSceneDataFromModel(model);
    m_localBounds = BuildLocalBounds(model, m_meshResources);
}

// Model のノード変換や境界情報を既存リソースへ同期します。
void ModelResource::SyncSceneDataFromModel(const Model& model)
{
    // ノード行列や material の最新値だけを同期し、GPU バッファ自体は再生成しない。
    const auto& meshes = model.GetMeshes();
    const auto& materials = model.GetMaterials();
    const auto& nodes = model.GetNodes();

    m_hasSkinnedMeshes = false;

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

        // 今フレームを更新する前に、現在値を prev へスナップショットする。
        // これがないと skinning シェーダの prev 計算が現フレーム行列で行われ、
        // motion vector がボーン動作分を反映できず FSR2 が暴れる (粒子状アーティファクト)。
        resource.prevNodeWorldTransform = resource.nodeWorldTransform;

        DirectX::XMFLOAT4X4 newNodeWorld = IdentityMatrix();
        if (resource.nodeIndex >= 0 && static_cast<size_t>(resource.nodeIndex) < nodes.size()) {
            newNodeWorld = nodes[resource.nodeIndex].worldTransform;
        }
        resource.nodeWorldTransform = newNodeWorld;

        const size_t boneCount = mesh.bones.size();
        const bool boneCountChanged = (resource.bones.size() != boneCount);
        if (boneCountChanged) {
            resource.bones.resize(boneCount);
        }
        if (boneCount > 0) {
            m_hasSkinnedMeshes = true;
        }

        for (size_t boneIndex = 0; boneIndex < boneCount; ++boneIndex)
        {
            const auto& bone = mesh.bones[boneIndex];
            BoneResource& boneResource = resource.bones[boneIndex];

            const int newNodeIndex = model.GetMeshBoneNodeIndex(
                static_cast<int>(meshIndex), static_cast<int>(boneIndex));
            const bool boneIdentityChanged =
                boneCountChanged ||
                boneResource.nodeIndex != newNodeIndex;

            // 今フレームを書き込む前に prev を確保する。
            boneResource.prevWorldTransform = boneResource.worldTransform;

            boneResource.nodeIndex = newNodeIndex;
            boneResource.offsetTransform = bone.offsetTransform;

            DirectX::XMFLOAT4X4 newBoneWorld = IdentityMatrix();
            if (boneResource.nodeIndex >= 0 && static_cast<size_t>(boneResource.nodeIndex) < nodes.size()) {
                newBoneWorld = nodes[boneResource.nodeIndex].worldTransform;
            }
            boneResource.worldTransform = newBoneWorld;

            // 新規 / リバインドされたボーンは prev を current に揃え、初フレームの
            // 偽の motion vector (ゼロ行列由来) を防ぐ。
            if (boneIdentityChanged) {
                boneResource.prevWorldTransform = newBoneWorld;
            }
        }

        // 初回呼び出し (RebuildFromModel 直後) で prevNodeWorldTransform が
        // ゼロ行列のまま残らないようにする。
        const float* p = &resource.prevNodeWorldTransform._11;
        bool prevNodeIsZero = true;
        for (int i = 0; i < 16; ++i) {
            if (p[i] != 0.0f) { prevNodeIsZero = false; break; }
        }
        if (prevNodeIsZero) {
            resource.prevNodeWorldTransform = newNodeWorld;
        }
    }

    m_localBounds = BuildLocalBounds(model, m_meshResources);
}

// 指定メッシュの頂点・インデックスバッファを更新します。
void ModelResource::SyncMeshBuffers(int meshIndex,
    const std::shared_ptr<IBuffer>& vertexBuffer,
    // CPU 側で差し替わったメッシュバッファを描画資源へ反映する。
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

// 指定メッシュの読み取り専用リソースを取得します。
const ModelResource::MeshResource* ModelResource::GetMeshResource(int meshIndex) const
{
    if (meshIndex < 0 || static_cast<size_t>(meshIndex) >= m_meshResources.size()) {
        return nullptr;
    }
    return &m_meshResources[meshIndex];
}

// 指定メッシュの編集可能リソースを取得します。
ModelResource::MeshResource* ModelResource::GetMeshResource(int meshIndex)
{
    if (meshIndex < 0 || static_cast<size_t>(meshIndex) >= m_meshResources.size()) {
        return nullptr;
    }
    return &m_meshResources[meshIndex];
}

// 指定メッシュの頂点・インデックスバッファをコマンドリストへバインドします。
bool ModelResource::BindMeshBuffers(ICommandList* commandList, int meshIndex) const
{
    // 描画パスは Model ではなく ModelResource 経由で VB/IB を取得する。
    const MeshResource* meshResource = GetMeshResource(meshIndex);
    if (!commandList || !meshResource || !meshResource->vertexBuffer || !meshResource->indexBuffer) {
        return false;
    }

    commandList->SetVertexBuffer(0, meshResource->vertexBuffer.get(), meshResource->vertexStride, 0);
    commandList->SetIndexBuffer(meshResource->indexBuffer.get(), IndexFormat::Uint32, 0);
    return true;
}

// 指定メッシュのインデックス数を返します。
uint32_t ModelResource::GetMeshIndexCount(int meshIndex) const
{
    const MeshResource* meshResource = GetMeshResource(meshIndex);
    return meshResource ? meshResource->indexCount : 0;
}
