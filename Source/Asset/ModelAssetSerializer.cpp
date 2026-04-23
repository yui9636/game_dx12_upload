#include "ModelAssetSerializer.h"

#include "Console/Logger.h"
#include "Model/Model.h"
#include "System/PathResolver.h"
#include "System/ResourceManager.h"

#include <meshoptimizer.h>

#include <cereal/archives/binary.hpp>
#include <cereal/cereal.hpp>
#include <cereal/types/string.hpp>
#include <cereal/types/vector.hpp>

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <vector>

// DirectX の基本型を cereal でシリアライズできるようにする。
namespace DirectX
{
    template<class Archive>
    void serialize(Archive& archive, XMUINT4& v)
    {
        archive(
            cereal::make_nvp("x", v.x),
            cereal::make_nvp("y", v.y),
            cereal::make_nvp("z", v.z),
            cereal::make_nvp("w", v.w));
    }

    template<class Archive>
    void serialize(Archive& archive, XMFLOAT2& v)
    {
        archive(
            cereal::make_nvp("x", v.x),
            cereal::make_nvp("y", v.y));
    }

    template<class Archive>
    void serialize(Archive& archive, XMFLOAT3& v)
    {
        archive(
            cereal::make_nvp("x", v.x),
            cereal::make_nvp("y", v.y),
            cereal::make_nvp("z", v.z));
    }

    template<class Archive>
    void serialize(Archive& archive, XMFLOAT4& v)
    {
        archive(
            cereal::make_nvp("x", v.x),
            cereal::make_nvp("y", v.y),
            cereal::make_nvp("z", v.z),
            cereal::make_nvp("w", v.w));
    }

    template<class Archive>
    void serialize(Archive& archive, XMFLOAT4X4& m)
    {
        archive(
            cereal::make_nvp("_11", m._11),
            cereal::make_nvp("_12", m._12),
            cereal::make_nvp("_13", m._13),
            cereal::make_nvp("_14", m._14),
            cereal::make_nvp("_21", m._21),
            cereal::make_nvp("_22", m._22),
            cereal::make_nvp("_23", m._23),
            cereal::make_nvp("_24", m._24),
            cereal::make_nvp("_31", m._31),
            cereal::make_nvp("_32", m._32),
            cereal::make_nvp("_33", m._33),
            cereal::make_nvp("_34", m._34),
            cereal::make_nvp("_41", m._41),
            cereal::make_nvp("_42", m._42),
            cereal::make_nvp("_43", m._43),
            cereal::make_nvp("_44", m._44));
    }
}

// Model::Node を cereal で保存・読込できるようにする。
template<class Archive>
void Model::Node::serialize(Archive& archive)
{
    archive(
        CEREAL_NVP(name),
        CEREAL_NVP(parentIndex),
        CEREAL_NVP(position),
        CEREAL_NVP(rotation),
        CEREAL_NVP(scale));
}

// Model::Material を cereal で保存・読込できるようにする。
template<class Archive>
void Model::Material::serialize(Archive& archive)
{
    archive(
        CEREAL_NVP(name),
        CEREAL_NVP(diffuseTextureFileName),
        CEREAL_NVP(normalTextureFileName),
        CEREAL_NVP(metallicTextureFileName),
        CEREAL_NVP(roughnessTextureFileName),
        CEREAL_NVP(albedoTextureFileName),
        CEREAL_NVP(occlusionTextureFileName),
        CEREAL_NVP(emissiveTextureFileName),
        CEREAL_NVP(metallicFactor),
        CEREAL_NVP(roughnessFactor),
        CEREAL_NVP(occlusionStrength),
        CEREAL_NVP(color));
}

// Model::Vertex を cereal で保存・読込できるようにする。
template<class Archive>
void Model::Vertex::serialize(Archive& archive)
{
    archive(
        CEREAL_NVP(position),
        CEREAL_NVP(boneWeight),
        CEREAL_NVP(boneIndex),
        CEREAL_NVP(texcoord),
        CEREAL_NVP(normal),
        CEREAL_NVP(tangent));
}

// Model::Bone を cereal で保存・読込できるようにする。
template<class Archive>
void Model::Bone::serialize(Archive& archive)
{
    archive(
        CEREAL_NVP(nodeIndex),
        CEREAL_NVP(offsetTransform));
}

// Model::Mesh を cereal で保存・読込できるようにする。
template<class Archive>
void Model::Mesh::serialize(Archive& archive)
{
    archive(
        CEREAL_NVP(vertices),
        CEREAL_NVP(indices),
        CEREAL_NVP(bones),
        CEREAL_NVP(nodeIndex),
        CEREAL_NVP(materialIndex));
}

// ベクトルキーを cereal で保存・読込できるようにする。
template<class Archive>
void Model::VectorKeyframe::serialize(Archive& archive)
{
    archive(
        CEREAL_NVP(seconds),
        CEREAL_NVP(value));
}

// クォータニオンキーを cereal で保存・読込できるようにする。
template<class Archive>
void Model::QuaternionKeyframe::serialize(Archive& archive)
{
    archive(
        CEREAL_NVP(seconds),
        CEREAL_NVP(value));
}

// ノード単位アニメーションを cereal で保存・読込できるようにする。
template<class Archive>
void Model::NodeAnim::serialize(Archive& archive)
{
    archive(
        CEREAL_NVP(positionKeyframes),
        CEREAL_NVP(rotationKeyframes),
        CEREAL_NVP(scaleKeyframes));
}

// アニメーション全体を cereal で保存・読込できるようにする。
template<class Archive>
void Model::Animation::serialize(Archive& archive)
{
    archive(
        CEREAL_NVP(name),
        CEREAL_NVP(secondsLength),
        CEREAL_NVP(nodeAnims));
}

namespace
{
    // シリアライザ入力として許可する元モデル形式かどうかを判定する。
    bool IsSupportedSourceModel(const std::filesystem::path& path)
    {
        std::string extension = path.extension().string();
        std::transform(extension.begin(), extension.end(), extension.begin(), [](unsigned char c) {
            return static_cast<char>(std::tolower(c));
            });

        return extension == ".fbx" || extension == ".obj" || extension == ".blend" || extension == ".gltf" || extension == ".glb";
    }

    // 保存・読込対象パスを絶対パス寄りの実体パスへ解決する。
    // 絶対パスならそのまま、相対パスなら PathResolver を通す。
    std::string ResolveSerializerPath(const std::string& path)
    {
        if (path.empty()) {
            return {};
        }

        const std::filesystem::path fsPath(path);
        if (fsPath.is_absolute()) {
            return fsPath.string();
        }

        return PathResolver::Resolve(path);
    }

    // 三角形インデックス列として有効な個数へ切り詰める。
    // 3 の倍数でない端数を落とす。
    size_t AlignTriangleIndexCount(size_t indexCount)
    {
        return indexCount - (indexCount % 3);
    }

    // meshoptimizer の simplifyWithAttributes 用に、
    // 法線・接線・UV を float 配列へ詰め直す。
    std::vector<float> BuildSimplifyAttributes(const std::vector<Model::Vertex>& vertices)
    {
        constexpr size_t kAttributeCount = 8;
        std::vector<float> attributes(vertices.size() * kAttributeCount, 0.0f);

        for (size_t i = 0; i < vertices.size(); ++i) {
            const Model::Vertex& vertex = vertices[i];
            float* attribute = attributes.data() + i * kAttributeCount;

            attribute[0] = vertex.normal.x;
            attribute[1] = vertex.normal.y;
            attribute[2] = vertex.normal.z;
            attribute[3] = vertex.tangent.x;
            attribute[4] = vertex.tangent.y;
            attribute[5] = vertex.tangent.z;
            attribute[6] = vertex.texcoord.x;
            attribute[7] = vertex.texcoord.y;
        }

        return attributes;
    }

    // 1メッシュぶんの簡略化・頂点キャッシュ最適化・オーバードロー最適化・頂点フェッチ最適化を行う。
    void OptimizeMesh(
        std::vector<Model::Vertex>& vertices,
        std::vector<uint32_t>& indices,
        bool hasSkinning,
        const ModelSerializerSettings& settings,
        bool& outSimplified)
    {
        outSimplified = false;

        // 三角形を組めないほど小さいなら何もしない。
        if (vertices.size() < 3 || indices.size() < 3) {
            return;
        }

        // 簡略化を希望し、かつ target ratio が十分小さい場合だけ実行候補にする。
        const bool wantsSimplification = settings.enableSimplification && settings.targetTriangleRatio < 0.999f;

        // スキニング付きメッシュは現状 simplify 対象外にする。
        if (wantsSimplification && !hasSkinning && indices.size() >= 6) {
            size_t targetIndexCount = AlignTriangleIndexCount(
                static_cast<size_t>(static_cast<double>(indices.size()) * std::clamp(settings.targetTriangleRatio, 0.01f, 1.0f)));

            targetIndexCount = (std::max)(targetIndexCount, static_cast<size_t>(3));
            targetIndexCount = (std::min)(targetIndexCount, AlignTriangleIndexCount(indices.size()));

            if (targetIndexCount >= 3 && targetIndexCount < indices.size()) {
                std::vector<uint32_t> simplified(indices.size());

                // 法線・接線・UV を追加属性として渡す。
                const std::vector<float> attributes = BuildSimplifyAttributes(vertices);
                const float attributeWeights[8] = { 0.75f, 0.75f, 0.75f, 0.35f, 0.35f, 0.35f, 0.5f, 0.5f };

                float resultError = 0.0f;
                const unsigned int options = settings.lockBorder ? meshopt_SimplifyLockBorder : 0;

                // 属性付き簡略化を実行する。
                size_t simplifiedCount = meshopt_simplifyWithAttributes(
                    simplified.data(),
                    indices.data(),
                    indices.size(),
                    reinterpret_cast<const float*>(&vertices[0].position.x),
                    vertices.size(),
                    sizeof(Model::Vertex),
                    attributes.data(),
                    sizeof(float) * 8,
                    attributeWeights,
                    8,
                    targetIndexCount,
                    settings.targetError,
                    options,
                    &resultError);

                simplifiedCount = AlignTriangleIndexCount(simplifiedCount);

                // 有効に簡略化できた場合のみ差し替える。
                if (simplifiedCount >= 3 && simplifiedCount < indices.size()) {
                    simplified.resize(simplifiedCount);
                    indices.swap(simplified);
                    outSimplified = true;
                }
            }
        }

        // 頂点キャッシュ最適化。
        if (settings.optimizeVertexCache && indices.size() >= 3) {
            std::vector<uint32_t> cacheOptimized(indices.size());
            meshopt_optimizeVertexCache(cacheOptimized.data(), indices.data(), indices.size(), vertices.size());
            indices.swap(cacheOptimized);
        }

        // オーバードロー最適化。
        if (settings.optimizeOverdraw && indices.size() >= 3) {
            std::vector<uint32_t> overdrawOptimized(indices.size());
            meshopt_optimizeOverdraw(
                overdrawOptimized.data(),
                indices.data(),
                indices.size(),
                reinterpret_cast<const float*>(&vertices[0].position.x),
                vertices.size(),
                sizeof(Model::Vertex),
                (std::max)(1.0f, settings.overdrawThreshold));
            indices.swap(overdrawOptimized);
        }

        // 頂点フェッチ最適化。
        if (settings.optimizeVertexFetch && indices.size() >= 3) {
            std::vector<Model::Vertex> fetchOptimized(vertices.size());
            const size_t optimizedVertexCount = meshopt_optimizeVertexFetch(
                fetchOptimized.data(),
                indices.data(),
                indices.size(),
                vertices.data(),
                vertices.size(),
                sizeof(Model::Vertex));
            fetchOptimized.resize(optimizedVertexCount);
            vertices.swap(fetchOptimized);
        }
    }

    // 出力先パスを決定する。
    // 明示指定が無ければ sourcePath をベースにし、拡張子は .cereal に置き換える。
    std::filesystem::path BuildOutputPath(const std::string& sourcePath, const std::string& requestedOutputPath)
    {
        std::filesystem::path output = requestedOutputPath.empty()
            ? std::filesystem::path(sourcePath)
            : std::filesystem::path(requestedOutputPath);

        output.replace_extension(".cereal");
        return output;
    }
}

// ソースモデルを読み込み、必要なら最適化し、.cereal 形式へ保存する。
ModelSerializerResult ModelAssetSerializer::Build(
    const std::string& sourcePath,
    const std::string& outputPath,
    const ModelSerializerSettings& settings)
{
    ModelSerializerResult result;
    result.sourcePath = sourcePath;
    result.outputPath = BuildOutputPath(sourcePath, outputPath).string();

    // ソースパス未指定はエラー。
    if (sourcePath.empty()) {
        result.message = "Source model path is empty.";
        return result;
    }

    // 対応拡張子以外は受け付けない。
    const std::filesystem::path sourceFsPath(sourcePath);
    if (!IsSupportedSourceModel(sourceFsPath)) {
        result.message = "Drop a source model asset (.fbx/.obj/.blend/.gltf/.glb).";
        return result;
    }

    // ソースパスを実体パスへ解決する。
    const std::string resolvedSource = ResolveSerializerPath(sourcePath);

    // ソースファイルが存在しないならエラー。
    if (!std::filesystem::exists(resolvedSource)) {
        result.message = "Source model file was not found.";
        LOG_WARN("[Serializer] Source model not found: %s", resolvedSource.c_str());
        return result;
    }

    // 出力パスを決定し、実体パスへ解決する。
    const std::filesystem::path outputFsPath = BuildOutputPath(sourcePath, outputPath);
    const std::string resolvedOutput = ResolveSerializerPath(outputFsPath.string());

    // 出力先ディレクトリを必要なら作成する。
    try {
        const std::filesystem::path parentPath = std::filesystem::path(resolvedOutput).parent_path();
        if (!parentPath.empty()) {
            std::filesystem::create_directories(parentPath);
        }
    }
    catch (const std::exception& e) {
        result.message = std::string("Failed to prepare output directory: ") + e.what();
        LOG_WARN("[Serializer] Output directory prepare failed: %s", e.what());
        return result;
    }

    // ソースモデルを読み込む。
    // settings.scaling を適用し、第三引数 true で読み込み時オプションを有効化している。
    Model workingModel(resolvedSource.c_str(), settings.scaling, true);

    // subset 数を処理メッシュ数として記録する。
    result.processedMeshCount = static_cast<size_t>(workingModel.GetSubsetCount());

    // 頂点数・インデックス数を集計する補助ラムダ。
    const auto accumulateCounts = [&](size_t& vertexCount, size_t& indexCount) {
        vertexCount = 0;
        indexCount = 0;
        for (const Model::Mesh& mesh : workingModel.GetMeshes()) {
            vertexCount += mesh.vertices.size();
            indexCount += mesh.indices.size();
        }
        };

    // 最適化前の総数を記録する。
    accumulateCounts(result.sourceVertexCount, result.sourceIndexCount);

    // 簡略化希望かどうかを先に判定しておく。
    const bool wantsSimplification = settings.enableSimplification && settings.targetTriangleRatio < 0.999f;

    // 各 subset を順に最適化する。
    for (int subsetIndex = 0; subsetIndex < workingModel.GetSubsetCount(); ++subsetIndex) {
        const bool hasSkinning = !workingModel.GetMeshes()[subsetIndex].bones.empty();

        // スキニング付きで簡略化対象外になった数を記録する。
        if (wantsSimplification && hasSkinning) {
            ++result.skippedSimplificationMeshCount;
        }

        // 編集可能な mesh data を取得する。
        Model::MeshData meshData = workingModel.GetMeshData(subsetIndex);
        if (!meshData.vertices || !meshData.indices) {
            continue;
        }

        bool simplified = false;
        OptimizeMesh(*meshData.vertices, *meshData.indices, hasSkinning, settings, simplified);

        if (simplified) {
            ++result.simplifiedMeshCount;
        }
    }

    // 最適化後の総数を記録する。
    accumulateCounts(result.outputVertexCount, result.outputIndexCount);

    // 出力ファイルを開く。
    std::ofstream stream(resolvedOutput, std::ios::binary);
    if (!stream.is_open()) {
        result.message = "Failed to open output .cereal file.";
        LOG_WARN("[Serializer] Failed to open output: %s", resolvedOutput.c_str());
        return result;
    }

    // cereal バイナリアーカイブで各データを保存する。
    try {
        cereal::BinaryOutputArchive archive(stream);
        archive(
            cereal::make_nvp("nodes", workingModel.GetNodes()),
            cereal::make_nvp("materials", workingModel.GetMaterialss()),
            cereal::make_nvp("meshes", workingModel.GetMeshes()),
            cereal::make_nvp("animations", workingModel.GetAnimations()));
    }
    catch (const std::exception& e) {
        result.message = std::string("Serialization failed: ") + e.what();
        LOG_WARN("[Serializer] Save failed: %s", e.what());
        return result;
    }
    catch (...) {
        result.message = "Serialization failed.";
        LOG_WARN("[Serializer] Save failed.");
        return result;
    }

    // ソースと出力のモデルキャッシュを無効化して、次回再読込を強制する。
    ResourceManager::Instance().InvalidateModel(sourcePath);
    ResourceManager::Instance().InvalidateModel(outputFsPath.string());

    result.success = true;
    result.message = "Serializer build completed.";

    // 結果をログへ出す。
    LOG_INFO(
        "[Serializer] Built '%s' -> '%s' meshes=%zu simplified=%zu indices=%zu->%zu vertices=%zu->%zu",
        resolvedSource.c_str(),
        resolvedOutput.c_str(),
        result.processedMeshCount,
        result.simplifiedMeshCount,
        result.sourceIndexCount,
        result.outputIndexCount,
        result.sourceVertexCount,
        result.outputVertexCount);

    return result;
}