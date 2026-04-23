#pragma once

#include <cstddef>
#include <string>

// モデルシリアライザのビルド設定。
// 元モデルを .cereal 化する時の縮尺や最適化方針をまとめて持つ。
struct ModelSerializerSettings
{
    // 読み込み時にモデルへ掛ける全体スケール。
    float scaling = 1.0f;

    // メッシュ簡略化を行うかどうか。
    bool enableSimplification = true;

    // 目標三角形比率。
    // 1.0 に近いほど元形状維持、低いほど軽量化を優先する。
    float targetTriangleRatio = 0.35f;

    // 簡略化時に許容する誤差。
    float targetError = 0.02f;

    // 境界エッジを固定するかどうか。
    // シルエット崩れを抑えたい場合に有効。
    bool lockBorder = true;

    // 頂点キャッシュ最適化を行うかどうか。
    bool optimizeVertexCache = true;

    // オーバードロー最適化を行うかどうか。
    bool optimizeOverdraw = true;

    // オーバードロー最適化の閾値。
    float overdrawThreshold = 1.05f;

    // 頂点フェッチ最適化を行うかどうか。
    bool optimizeVertexFetch = true;
};

// モデルシリアライズ実行結果。
// 成否、入力出力パス、処理統計、メッセージをまとめて返す。
struct ModelSerializerResult
{
    // 成功したかどうか。
    bool success = false;

    // 元モデルの入力パス。
    std::string sourcePath;

    // 出力された .cereal ファイルのパス。
    std::string outputPath;

    // 実行結果メッセージ。
    std::string message;

    // 処理対象となったメッシュ数。
    size_t processedMeshCount = 0;

    // 実際に簡略化されたメッシュ数。
    size_t simplifiedMeshCount = 0;

    // スキニング等の理由で簡略化をスキップしたメッシュ数。
    size_t skippedSimplificationMeshCount = 0;

    // 最適化前の総頂点数。
    size_t sourceVertexCount = 0;

    // 最適化前の総インデックス数。
    size_t sourceIndexCount = 0;

    // 最適化後の総頂点数。
    size_t outputVertexCount = 0;

    // 最適化後の総インデックス数。
    size_t outputIndexCount = 0;
};

// モデルアセットを .cereal 形式へ変換するシリアライザ。
class ModelAssetSerializer
{
public:
    // sourcePath の元モデルを読み込み、必要に応じて最適化したうえで
    // outputPath へ .cereal 形式で保存する。
    // 結果は ModelSerializerResult として返す。
    static ModelSerializerResult Build(
        const std::string& sourcePath,
        const std::string& outputPath,
        const ModelSerializerSettings& settings = {});
};