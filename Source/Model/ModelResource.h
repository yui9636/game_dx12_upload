#pragma once
// モデル描画に使うメッシュ単位の GPU リソースを保持するクラス定義です。

#include <cstdint>
#include <memory>
#include <vector>
#include <DirectXCollision.h>
#include "Model.h"

class IBuffer;
class ICommandList;
class IResourceFactory;

// Model の内容を GPU 描画しやすいリソース形式で保持します。
class ModelResource
{
public:
    // 描画時に使うボーンのノード番号と行列情報を保持します。
    // worldTransform は今フレーム、prevWorldTransform は前フレームの bone world です。
    // FSR2 / TAA に渡す motion vector を正しく出すために前フレームの行列を保持します。
    struct BoneResource
    {
        int nodeIndex = -1;
        DirectX::XMFLOAT4X4 offsetTransform = {};
        DirectX::XMFLOAT4X4 worldTransform = {};
        DirectX::XMFLOAT4X4 prevWorldTransform = {};
    };

    // 1 メッシュ分の GPU バッファ・材質・ボーン情報を保持します。
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
        DirectX::XMFLOAT4X4 prevNodeWorldTransform = {};
        DirectX::BoundingBox localBounds = {};
        std::vector<BoneResource> bones;
    };

    ModelResource() = default;
    ~ModelResource() = default;

    // Model から描画用リソースを再構築します。
    void RebuildFromModel(const Model& model, IResourceFactory* factory);
    // シーン側の変換情報を描画リソースへ同期します。
    void SyncSceneDataFromModel(const Model& model);
    // 指定メッシュの GPU バッファを同期します。
    void SyncMeshBuffers(int meshIndex,
        const std::shared_ptr<IBuffer>& vertexBuffer,
        const std::shared_ptr<IBuffer>& indexBuffer,
        uint32_t vertexStride,
        uint32_t indexCount,
        int materialIndex,
        int nodeIndex);

    // 指定メッシュの読み取り専用リソースを取得します。
    const MeshResource* GetMeshResource(int meshIndex) const;
    // 指定メッシュの編集可能リソースを取得します。
    MeshResource* GetMeshResource(int meshIndex);
    // 指定メッシュのバッファを描画用にバインドします。
    bool BindMeshBuffers(ICommandList* commandList, int meshIndex) const;
    // 指定メッシュのインデックス数を取得します。
    uint32_t GetMeshIndexCount(int meshIndex) const;
    // 保持しているメッシュ数を取得します。
    int GetMeshCount() const { return static_cast<int>(m_meshResources.size()); }

    // 全メッシュリソースを読み取り専用で取得します。
    const std::vector<MeshResource>& GetMeshResources() const { return m_meshResources; }
    // モデル全体のローカル境界を取得します。
    const DirectX::BoundingBox& GetLocalBounds() const { return m_localBounds; }
    // スキニングメッシュを含むかどうかを取得します。
    bool HasSkinnedMeshes() const { return m_hasSkinnedMeshes; }

private:
    std::vector<MeshResource> m_meshResources;
    DirectX::BoundingBox m_localBounds = {};
    bool m_hasSkinnedMeshes = false;
};
