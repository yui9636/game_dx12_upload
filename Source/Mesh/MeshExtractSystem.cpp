#include "MeshExtractSystem.h"
#include "Model/Model.h"
#include "Model/ModelResource.h"
#include "Material/MaterialAsset.h"
#include "System/ResourceManager.h"
#include "System/TaskSystem.h"
#include "Console/Logger.h"
#include "Type/TypeInfo.h"
#include <algorithm>
#include <chrono>
#include <cstring>
#include <unordered_map>

namespace
{
    // 既存のハッシュ値に新しい値を混ぜ込み、複数項目から 1 つのハッシュを作る。
    inline void HashCombine(uint64_t& seed, uint64_t value)
    {
        seed ^= value + 0x9e3779b97f4a7c15ull + (seed << 6) + (seed >> 2);
    }

    // float のビット列をそのまま uint32_t として扱い、マテリアル差分判定用のハッシュに変換する。
    inline uint64_t HashFloat(float value)
    {
        uint32_t bits = 0;
        static_assert(sizeof(bits) == sizeof(value));
        std::memcpy(&bits, &value, sizeof(bits));
        return std::hash<uint32_t>()(bits);
    }

    // 同じ描画設定・同じテクスチャ構成のマテリアルを同一グループとして扱うためのハッシュを作る。
    uint64_t BuildMaterialGroupHash(const MaterialAsset& material)
    {
        uint64_t seed = 0;
        HashCombine(seed, std::hash<int>()(material.shaderId));
        HashCombine(seed, std::hash<int>()(material.alphaMode));
        HashCombine(seed, HashFloat(material.baseColor.x));
        HashCombine(seed, HashFloat(material.baseColor.y));
        HashCombine(seed, HashFloat(material.baseColor.z));
        HashCombine(seed, HashFloat(material.baseColor.w));
        HashCombine(seed, HashFloat(material.metallic));
        HashCombine(seed, HashFloat(material.roughness));
        HashCombine(seed, HashFloat(material.emissive));
        HashCombine(seed, std::hash<std::string>()(material.diffuseTexturePath));
        HashCombine(seed, std::hash<std::string>()(material.normalTexturePath));
        HashCombine(seed, std::hash<std::string>()(material.metallicRoughnessTexturePath));
        HashCombine(seed, std::hash<std::string>()(material.emissiveTexturePath));
        return seed;
    }
}

// ECS 上の Mesh / Transform / Material から描画用の RenderPacket と InstanceBatch を構築する。
void MeshExtractSystem::Extract(Registry& registry, RenderQueue& queue)
{
    using Clock = std::chrono::high_resolution_clock;
    const auto startTime = Clock::now();

    // MaterialComponent を持たないメッシュに使うデフォルトマテリアルを取得する。
    auto defaultMat = ResourceManager::Instance().GetDefaultMaterial();
    m_sources.clear();

    // MeshComponent と TransformComponent を両方持つ Archetype だけを抽出対象にする。
    const Signature querySignature = CreateSignature<MeshComponent, TransformComponent>();
    const ComponentTypeID meshType = TypeManager::GetComponentTypeID<MeshComponent>();
    const ComponentTypeID transformType = TypeManager::GetComponentTypeID<TransformComponent>();
    const ComponentTypeID materialType = TypeManager::GetComponentTypeID<MaterialComponent>();

    // パス文字列から MaterialAsset を解決した回数。プロファイル表示用に記録する。
    uint32_t materialResolveCount = 0;

    // Registry 内の全 Archetype を走査し、描画に必要な component のポインタだけを m_sources に集める。
    for (Archetype* archetype : registry.GetAllArchetypes()) {
        if (!archetype || !SignatureMatches(archetype->GetSignature(), querySignature)) {
            continue;
        }

        const size_t entityCount = archetype->GetEntityCount();
        if (entityCount == 0) {
            continue;
        }

        auto* meshColumn = archetype->GetColumn(meshType);
        auto* transformColumn = archetype->GetColumn(transformType);
        auto* materialColumn = archetype->GetColumn(materialType);
        if (!meshColumn || !transformColumn) {
            continue;
        }

        auto* meshes = static_cast<MeshComponent*>(meshColumn->Get(0));
        auto* transforms = static_cast<TransformComponent*>(transformColumn->Get(0));
        auto* materials = materialColumn ? static_cast<MaterialComponent*>(materialColumn->Get(0)) : nullptr;

        // ComponentColumn は連続配列なので、先頭ポインタから entity 数分だけまとめて参照する。
        const size_t beginIndex = m_sources.size();
        m_sources.resize(beginIndex + entityCount);
        for (size_t i = 0; i < entityCount; ++i) {
            auto& source = m_sources[beginIndex + i];
            source.mesh = &meshes[i];
            source.transform = &transforms[i];
            source.material = materials ? &materials[i] : nullptr;

            // MaterialAsset が未解決で path だけ持っている場合は、この段階で ResourceManager から取得する。
            if (source.material && !source.material->materialAsset && !source.material->materialAssetPath.empty()) {
                source.material->materialAsset = ResourceManager::Instance().GetMaterial(source.material->materialAssetPath);
                ++materialResolveCount;
            }
        }
    }

    // 予想される描画対象数に対して queue 側の vector が足りない場合、容量拡張が起きる回数として記録する。
    const size_t expectedVisibleCount = m_sources.size();
    if (queue.opaquePackets.capacity() < expectedVisibleCount) {
        ++queue.metrics.opaquePacketVectorGrowths;
    }
    if (queue.transparentPackets.capacity() < expectedVisibleCount) {
        ++queue.metrics.transparentPacketVectorGrowths;
    }

    // 並列抽出用のバケットを source 数に合わせて確保し、前フレームの中身を消す。
    m_buckets.resize(expectedVisibleCount);
    for (size_t i = 0; i < expectedVisibleCount; ++i) {
        m_buckets[i].opaquePackets.clear();
        m_buckets[i].transparentPackets.clear();
    }

    // 各 source を並列に RenderPacket へ変換する。
    TaskSystem::Instance().ParallelFor(
        expectedVisibleCount,
        32,
        [&](size_t sourceIndex) {
            const auto& source = m_sources[sourceIndex];
            if (!source.mesh || !source.transform) {
                return;
            }

            MeshComponent& mesh = *source.mesh;
            if (!mesh.isVisible || !mesh.model) {
                return;
            }

            // MaterialComponent が無い、または MaterialAsset が無い場合はデフォルトマテリアルを使う。
            MaterialAsset* activeMat = defaultMat.get();
            if (source.material && source.material->materialAsset) {
                activeMat = source.material->materialAsset.get();
            }

            // RenderPass が直接扱う描画パケットを作成する。
            RenderPacket packet;
            packet.modelResource = mesh.model->GetModelResource();
            packet.worldMatrix = source.transform->worldMatrix;
            packet.prevWorldMatrix = source.transform->prevWorldMatrix;
            packet.castShadow = mesh.castShadow;
            packet.shaderId = activeMat->shaderId;
            packet.baseColor = activeMat->baseColor;
            packet.metallic = activeMat->metallic;
            packet.roughness = activeMat->roughness;
            packet.emissive = activeMat->emissive;
            packet.materialAsset = source.material && source.material->materialAsset
                ? source.material->materialAsset
                : defaultMat;
            packet.materialGroupHash = BuildMaterialGroupHash(*activeMat);

            // alphaMode == 2 を透過として扱い、不透明と透過で別 queue に分ける。
            auto& bucket = m_buckets[sourceIndex];
            const bool isTransparent = (activeMat->alphaMode == 2);
            if (isTransparent) {
                bucket.transparentPackets.push_back(std::move(packet));
            } else {
                bucket.opaquePackets.push_back(std::move(packet));
            }
        });

    // RenderQueue の前フレームの抽出結果を消し、今回の最大数を見込んで容量を確保する。
    queue.opaquePackets.clear();
    queue.transparentPackets.clear();
    queue.opaquePackets.reserve(expectedVisibleCount);
    queue.transparentPackets.reserve(expectedVisibleCount);

    // 並列処理で作った各バケットを、最終的な不透明・透過 packet 配列へ集約する。
    for (auto& bucket : m_buckets) {
        if (!bucket.opaquePackets.empty()) {
            queue.opaquePackets.insert(
                queue.opaquePackets.end(),
                std::make_move_iterator(bucket.opaquePackets.begin()),
                std::make_move_iterator(bucket.opaquePackets.end()));
        }
        if (!bucket.transparentPackets.empty()) {
            queue.transparentPackets.insert(
                queue.transparentPackets.end(),
                std::make_move_iterator(bucket.transparentPackets.begin()),
                std::make_move_iterator(bucket.transparentPackets.end()));
        }
    }

    // 不透明パケットを同一描画条件ごとの InstanceBatch にまとめる。
    m_batchLookup.clear();
    m_batchLookup.reserve(queue.opaquePackets.size());
    if (queue.opaqueInstanceBatches.capacity() < queue.opaquePackets.size()) {
        ++queue.metrics.opaqueBatchVectorGrowths;
    }
    queue.opaqueInstanceBatches.reserve(queue.opaquePackets.size());

    // skinned / non-skinned の数をメトリクス用に数えながら、バッチキーを作る。
    uint32_t nonSkinnedOpaquePacketCount = 0;
    uint32_t skinnedOpaquePacketCount = 0;
    for (const RenderPacket& packet : queue.opaquePackets) {
        const bool isSkinnedPacket = packet.modelResource && packet.modelResource->HasSkinnedMeshes();
        if (isSkinnedPacket) {
            ++skinnedOpaquePacketCount;
        } else {
            ++nonSkinnedOpaquePacketCount;
        }

        // 同じ model / material / shader / render state の packet を同じ InstanceBatch にまとめるためのキー。
        DrawBatchKey batchKey{};
        batchKey.modelResource = packet.modelResource.get();
        batchKey.shaderId = packet.shaderId;
        batchKey.castShadow = packet.castShadow;
        batchKey.blendState = packet.blendState;
        batchKey.depthState = packet.depthState;
        batchKey.rasterizerState = packet.rasterizerState;
        batchKey.baseColor = packet.baseColor;
        batchKey.metallic = packet.metallic;
        batchKey.roughness = packet.roughness;
        batchKey.emissive = packet.emissive;
        batchKey.materialAsset = packet.materialAsset.get();
        batchKey.materialGroupHash = packet.materialGroupHash;

        // 初めて見るキーなら新しいバッチを作り、既存ならそのバッチに instance を追加する。
        auto [it, inserted] = m_batchLookup.emplace(batchKey, queue.opaqueInstanceBatches.size());
        if (inserted) {
            InstanceBatch batch{};
            batch.key = batchKey;
            batch.modelResource = packet.modelResource;
            queue.opaqueInstanceBatches.push_back(std::move(batch));
        }

        // GPU instance buffer に渡す 1 個分のワールド行列情報。
        InstanceData instance{};
        instance.worldMatrix = packet.worldMatrix;
        instance.prevWorldMatrix = packet.prevWorldMatrix;
        queue.opaqueInstanceBatches[it->second].instances.push_back(instance);
    }

    // 描画順と PSO / material 切り替えを安定させるため、バッチを shader → material → model の順で並べる。
    const auto sortStart = Clock::now();
    std::sort(
        queue.opaqueInstanceBatches.begin(),
        queue.opaqueInstanceBatches.end(),
        [](const InstanceBatch& lhs, const InstanceBatch& rhs) {
            if (lhs.key.shaderId != rhs.key.shaderId) {
                return lhs.key.shaderId < rhs.key.shaderId;
            }
            if (lhs.key.materialGroupHash != rhs.key.materialGroupHash) {
                return lhs.key.materialGroupHash < rhs.key.materialGroupHash;
            }
            return lhs.modelResource.get() < rhs.modelResource.get();
        });
    const auto sortEnd = Clock::now();

    // 1 バッチ内に含まれる最大インスタンス数を計測する。
    uint32_t maxInstancesPerBatch = 0;
    for (const InstanceBatch& batch : queue.opaqueInstanceBatches) {
        const uint32_t count = static_cast<uint32_t>(batch.instances.size());
        if (count > maxInstancesPerBatch) {
            maxInstancesPerBatch = count;
        }
    }

    // Mesh 抽出の各種メトリクスを RenderQueue に保存する。
    queue.metrics.meshExtractMs =
        std::chrono::duration<double, std::milli>(Clock::now() - startTime).count();
    queue.metrics.materialResolveCount = materialResolveCount;
    queue.metrics.materialGroupCount = static_cast<uint32_t>(m_batchLookup.size());
    queue.metrics.opaquePacketCount = static_cast<uint32_t>(queue.opaquePackets.size());
    queue.metrics.transparentPacketCount = static_cast<uint32_t>(queue.transparentPackets.size());
    queue.metrics.opaqueBatchCount = static_cast<uint32_t>(queue.opaqueInstanceBatches.size());
    queue.metrics.maxInstancesPerBatch = maxInstancesPerBatch;
    queue.metrics.nonSkinnedOpaquePacketCount = nonSkinnedOpaquePacketCount;
    queue.metrics.skinnedOpaquePacketCount = skinnedOpaquePacketCount;
    queue.metrics.batchSortMs =
        std::chrono::duration<double, std::milli>(sortEnd - sortStart).count();

    // 抽出数が変わったときだけログを出し、毎フレーム同じログが流れないようにする。
    static size_t s_lastOpaqueCount = static_cast<size_t>(-1);
    static size_t s_lastTransparentCount = static_cast<size_t>(-1);
    static size_t s_lastBatchCount = static_cast<size_t>(-1);
    if (s_lastOpaqueCount != queue.opaquePackets.size() ||
        s_lastTransparentCount != queue.transparentPackets.size() ||
        s_lastBatchCount != queue.opaqueInstanceBatches.size()) {
        LOG_INFO("[MeshExtract] opaque=%zu transparent=%zu batches=%zu",
            queue.opaquePackets.size(), queue.transparentPackets.size(), queue.opaqueInstanceBatches.size());
        s_lastOpaqueCount = queue.opaquePackets.size();
        s_lastTransparentCount = queue.transparentPackets.size();
        s_lastBatchCount = queue.opaqueInstanceBatches.size();
    }
}
