#include "ModelSerializerPanel.h"

#include "Engine/EditorSelection.h"

#include <imgui.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <cstring>

namespace
{
    // InputText に渡すパス文字列バッファの固定サイズ。
    constexpr size_t kPathBufferSize = 512;

    // std::string の内容を固定長 char バッファへ安全にコピーする。
    // 末尾は必ず '\0' で終端する。
    void CopyStringToBuffer(const std::string& value, std::array<char, kPathBufferSize>& buffer)
    {
        // いったん全体をゼロ埋めする。
        buffer.fill('\0');

        // 空文字列なら何もしない。
        if (value.empty()) {
            return;
        }

        // バッファ末尾の終端文字ぶんを残してコピー長を決める。
        const size_t copyLength = (std::min)(value.size(), buffer.size() - 1);

        // 文字列本体をコピーする。
        memcpy(buffer.data(), value.data(), copyLength);

        // 念のため終端を明示する。
        buffer[copyLength] = '\0';
    }
}

// ドロップされたアセットを serializer の入力元として受け入れる。
// 対応拡張子なら source/output を更新し、非対応なら結果欄へエラーを出す。
bool ModelSerializerPanel::AcceptSourceAsset(const std::filesystem::path& path)
{
    // 対応していない拡張子ならエラーメッセージを設定して終了する。
    if (!IsSupportedSourceAsset(path)) {
        m_hasResult = true;
        m_lastResult = {};
        m_lastResult.message = "Source model only: .fbx / .obj / .blend / .gltf / .glb";
        return false;
    }

    // 入力元モデルパスを保存する。
    m_sourcePath = path;

    // 出力先は同名 .cereal を初期値として設定する。
    m_outputPath = BuildDefaultOutputPath(path).string();

    return true;
}

// serializer の入力元として受け付ける拡張子かどうかを判定する。
bool ModelSerializerPanel::IsSupportedSourceAsset(const std::filesystem::path& path)
{
    // 拡張子を文字列で取得する。
    std::string extension = path.extension().string();

    // 大文字小文字差を消すため小文字化する。
    std::transform(extension.begin(), extension.end(), extension.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
        });

    // 対応モデル形式のみ true を返す。
    return extension == ".fbx" || extension == ".obj" || extension == ".blend" || extension == ".gltf" || extension == ".glb";
}

// 入力元モデルパスから、既定の出力 .cereal パスを作る。
std::filesystem::path ModelSerializerPanel::BuildDefaultOutputPath(const std::filesystem::path& sourcePath)
{
    // 元パスをベースにする。
    std::filesystem::path outputPath = sourcePath;

    // 拡張子だけ .cereal に置き換える。
    outputPath.replace_extension(".cereal");

    return outputPath;
}

// serializer パネル全体を描画する。
void ModelSerializerPanel::Draw(bool* p_open, bool* outFocused)
{
    // ウィンドウを開く。折りたたまれている場合はフォーカスだけ返して終了する。
    if (!ImGui::Begin("Serializer", p_open)) {
        if (outFocused) {
            *outFocused = false;
        }
        ImGui::End();
        return;
    }

    // 現在ウィンドウがフォーカスされているかを返す。
    if (outFocused) {
        *outFocused = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);
    }

    // パネルの説明文を表示する。
    ImGui::TextWrapped("Drop a source model from the Asset Browser and build a manual .cereal serializer.");

    // 現在選択中アセットを source に使うボタン。
    if (ImGui::Button("Use Selected Asset")) {
        const EditorSelection& selection = EditorSelection::Instance();
        if (selection.GetType() == SelectionType::Asset) {
            AcceptSourceAsset(selection.GetAssetPath());
        }
    }

    ImGui::SameLine();

    // source / output をクリアするボタン。
    if (ImGui::Button("Clear")) {
        m_sourcePath.clear();
        m_outputPath.clear();
    }

    ImGui::Separator();

    // Source パス表示用の読み取り専用テキスト欄。
    std::array<char, kPathBufferSize> sourceBuffer{};
    CopyStringToBuffer(m_sourcePath.string(), sourceBuffer);
    ImGui::InputText("Source", sourceBuffer.data(), sourceBuffer.size(), ImGuiInputTextFlags_ReadOnly);

    // Asset Browser からのドラッグ&ドロップを受け付ける。
    if (ImGui::BeginDragDropTarget()) {
        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ENGINE_ASSET")) {
            AcceptSourceAsset(static_cast<const char*>(payload->Data));
        }
        ImGui::EndDragDropTarget();
    }

    // Output パスは編集可能にする。
    std::array<char, kPathBufferSize> outputBuffer{};
    CopyStringToBuffer(m_outputPath, outputBuffer);
    if (ImGui::InputText("Output", outputBuffer.data(), outputBuffer.size())) {
        m_outputPath = outputBuffer.data();
    }

    ImGui::Spacing();

    // 簡略化設定 UI。
    ImGui::Checkbox("Simplify static meshes", &m_settings.enableSimplification);
    if (m_settings.enableSimplification) {
        ImGui::SliderFloat("Triangle Ratio", &m_settings.targetTriangleRatio, 0.05f, 1.0f, "%.2f");
        ImGui::DragFloat("Target Error", &m_settings.targetError, 0.001f, 0.0f, 1.0f, "%.3f");
        ImGui::Checkbox("Lock Border", &m_settings.lockBorder);
    }

    // 頂点キャッシュ最適化設定。
    ImGui::Checkbox("Optimize Vertex Cache", &m_settings.optimizeVertexCache);

    // オーバードロー最適化設定。
    ImGui::Checkbox("Optimize Overdraw", &m_settings.optimizeOverdraw);
    if (m_settings.optimizeOverdraw) {
        ImGui::SliderFloat("Overdraw Threshold", &m_settings.overdrawThreshold, 1.0f, 3.0f, "%.2f");
    }

    // 頂点フェッチ最適化設定。
    ImGui::Checkbox("Optimize Vertex Fetch", &m_settings.optimizeVertexFetch);

    // 全体スケール設定。
    ImGui::DragFloat("Scaling", &m_settings.scaling, 0.01f, 0.001f, 100.0f, "%.3f");

    ImGui::Spacing();

    // スキニングメッシュ簡略化は危険なので現状スキップする旨を表示する。
    ImGui::TextDisabled("Skinned mesh simplification is skipped automatically for safety.");

    // source が有効で対応拡張子なら build を許可する。
    const bool canBuild = !m_sourcePath.empty() && IsSupportedSourceAsset(m_sourcePath);
    if (!canBuild) {
        ImGui::BeginDisabled();
    }

    // Build 実行ボタン。
    if (ImGui::Button("Build Serializer", ImVec2(180.0f, 0.0f))) {
        m_lastResult = ModelAssetSerializer::Build(m_sourcePath.string(), m_outputPath, m_settings);
        m_hasResult = true;
    }

    if (!canBuild) {
        ImGui::EndDisabled();
    }

    // 前回ビルド結果があるなら結果欄を表示する。
    if (m_hasResult) {
        ImGui::Separator();

        // 成功なら緑、失敗なら赤でメッセージ表示する。
        const ImVec4 color = m_lastResult.success
            ? ImVec4(0.45f, 0.95f, 0.55f, 1.0f)
            : ImVec4(1.0f, 0.45f, 0.45f, 1.0f);

        ImGui::TextColored(color, "%s", m_lastResult.message.c_str());

        // 出力先パスがあれば表示する。
        if (!m_lastResult.outputPath.empty()) {
            ImGui::TextWrapped("Output: %s", m_lastResult.outputPath.c_str());
        }

        // 処理統計を表示する。
        ImGui::Text("Meshes: %zu", m_lastResult.processedMeshCount);
        ImGui::Text("Simplified: %zu", m_lastResult.simplifiedMeshCount);
        ImGui::Text("Skipped(skin): %zu", m_lastResult.skippedSimplificationMeshCount);
        ImGui::Text("Indices: %zu -> %zu", m_lastResult.sourceIndexCount, m_lastResult.outputIndexCount);
        ImGui::Text("Vertices: %zu -> %zu", m_lastResult.sourceVertexCount, m_lastResult.outputVertexCount);
    }

    ImGui::End();
}