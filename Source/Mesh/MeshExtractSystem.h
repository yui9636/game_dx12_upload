#pragma once
#include "Registry/Registry.h"
#include "System/Query.h"
#include "Component/MeshComponent.h"
#include "Component/TransformComponent.h"
#include "RenderContext/RenderQueue.h"
#include "Component/MaterialComponent.h"
#include <unordered_map>
#include <vector>

// Registry 内の MeshComponent と TransformComponent を走査し、描画用の RenderQueue に変換するシステム。
// ECS のデータを描画パイプラインが扱いやすい RenderPacket / InstanceBatch にまとめる役割を持つ。
class MeshExtractSystem {
public:
    // Registry から描画対象メッシュを抽出し、RenderQueue の不透明・透過パケットとインスタンスバッチを更新する。
    void Extract(Registry& registry, RenderQueue& queue);

private:
    // 1 つのエンティティから抽出した、描画パケット作成に必要な元データ。
    struct ExtractSource
    {
        // 描画対象のメッシュ情報。
        MeshComponent* mesh = nullptr;

        // ワールド行列などを持つ Transform 情報。
        const TransformComponent* transform = nullptr;

        // 任意のマテリアル情報。存在しない場合はデフォルトマテリアルを使う。
        MaterialComponent* material = nullptr;
    };

    // 並列処理中に各 source ごとの抽出結果を一時的に受け取るバケット。
    struct ExtractBucket
    {
        // 不透明描画用の一時 RenderPacket。
        std::vector<RenderPacket> opaquePackets;

        // 透過描画用の一時 RenderPacket。
        std::vector<RenderPacket> transparentPackets;
    };

    // 今フレームで見つかった抽出元データ一覧。
    std::vector<ExtractSource> m_sources;

    // 並列抽出結果を source 単位で格納する一時バケット。
    std::vector<ExtractBucket> m_buckets;

    // 同じ描画条件を持つ不透明パケットを InstanceBatch にまとめるための検索表。
    std::unordered_map<DrawBatchKey, size_t, DrawBatchKeyHash> m_batchLookup;
};
