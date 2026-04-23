#pragma once

#include "Asset/ModelAssetSerializer.h"

#include <filesystem>
#include <string>

// モデルを .cereal 化するための手動 serializer パネル。
// ソースモデルの指定、出力先設定、最適化設定、ビルド実行、結果表示を担当する。
class ModelSerializerPanel
{
public:
    // パネル全体を描画する。
    // p_open が指定されていればウィンドウ開閉状態を受け取り、
    // outFocused が指定されていればフォーカス状態を返す。
    void Draw(bool* p_open = nullptr, bool* outFocused = nullptr);

private:
    // 指定パスを入力元アセットとして受け入れる。
    // 対応拡張子なら source/output を更新し、非対応なら結果欄へエラーを出す。
    bool AcceptSourceAsset(const std::filesystem::path& path);

    // serializer の入力元として受け付ける拡張子かどうかを判定する。
    static bool IsSupportedSourceAsset(const std::filesystem::path& path);

    // 入力元モデルパスから既定の出力 .cereal パスを作る。
    static std::filesystem::path BuildDefaultOutputPath(const std::filesystem::path& sourcePath);

    // 現在選択されている入力元モデルパス。
    std::filesystem::path m_sourcePath;

    // 出力先 .cereal パス文字列。
    std::string m_outputPath;

    // serializer 実行時の最適化・変換設定。
    ModelSerializerSettings m_settings;

    // 前回ビルド結果。
    ModelSerializerResult m_lastResult;

    // 前回結果が有効かどうか。
    bool m_hasResult = false;
};