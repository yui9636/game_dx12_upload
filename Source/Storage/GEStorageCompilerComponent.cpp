#include "Storage/GEStorageCompilerComponent.h"
#include "JSONManager.h"
#include "System/Dialog.h"      
#include "imgui.h"
#include <filesystem>
#include <fstream>

namespace fs = std::filesystem;

// モデル名から保存 (カーブ対応)
void GEStorageCompilerComponent::SaveGameplayData(const std::string& modelName,
    const std::vector<std::vector<GESequencerItem>>& timelines,
    const std::vector<GECurveSettings>& curves)
{
    if (modelName.empty()) return;
    SaveGameplayDataToPath(GetFilePath(modelName), timelines, curves);
}

// パス指定で保存 (カーブ対応)
void GEStorageCompilerComponent::SaveGameplayDataToPath(const std::string& fullPath,
    const std::vector<std::vector<GESequencerItem>>& timelines,
    const std::vector<GECurveSettings>& curves)
{
    if (fullPath.empty()) return;

    GameplayAsset asset;
    asset.timelines = timelines;
    asset.curves = curves; // ★忘れずにセット！

    fs::path dir = fs::path(fullPath).parent_path();
    if (!fs::exists(dir) && !dir.empty()) {
        fs::create_directories(dir);
    }

    JSONManager json(fullPath);
    json.Set("gameplayAsset", asset);
    json.Save();
}

// モデル名から読み込み (GameplayAsset全体を返す)
GameplayAsset GEStorageCompilerComponent::LoadGameplayData(const std::string& modelName)
{
    if (modelName.empty()) return {};
    return LoadGameplayDataFromPath(GetFilePath(modelName));
}

// パス指定で読み込み (GameplayAsset全体を返す)
GameplayAsset GEStorageCompilerComponent::LoadGameplayDataFromPath(const std::string& fullPath)
{
    if (fullPath.empty() || !fs::exists(fullPath)) return {};

    JSONManager json(fullPath);
    // マクロのおかげで timelines と curves が自動的に復元される
    return json.Get<GameplayAsset>("gameplayAsset", GameplayAsset{});
}

std::string GEStorageCompilerComponent::GetFilePath(const std::string& modelName) const
{
    return "Data/Gameplay/" + modelName + "_Gameplay.json";
}

void GEStorageCompilerComponent::OnGUI()
{
    ImGui::TextDisabled("Gameplay Data Source");

    // パスを表示・編集するテキストボックス
    char buf[256];
    strcpy_s(buf, targetFilePath.c_str());
    if (ImGui::InputText("Path", buf, sizeof(buf))) {
        targetFilePath = buf;
    }

    ImGui::SameLine();

    // ファイル選択ボタン
    if (ImGui::Button("..."))
    {
        char filepath[MAX_PATH] = "";
        // JSONファイルだけ選べるようにフィルタリング
        if (Dialog::OpenFileName(filepath, MAX_PATH, "Gameplay JSON\0*.json\0All Files\0*.*\0") == DialogResult::OK)
        {
            // 絶対パスを相対パスに変換するとポータビリティが上がりますが、まずはそのまま入れます
            // 必要なら fs::relative 等を使ってください
            targetFilePath = filepath;
        }
    }

    // ファイルが存在するかチェックして表示
    if (!targetFilePath.empty())
    {
        if (fs::exists(targetFilePath)) {
            ImGui::TextColored(ImVec4(0, 1, 0, 1), "File Found!");
        }
        else {
            ImGui::TextColored(ImVec4(1, 0, 0, 1), "File Not Found");
        }
    }
}


void GEStorageCompilerComponent::Serialize(json& outJson) const
{
    // コンポーネントの設定としてファイルパスを保存
    outJson["targetFilePath"] = targetFilePath;
}

void GEStorageCompilerComponent::Deserialize(const json& inJson)
{
    // ファイルパスを復元
    targetFilePath = inJson.value("targetFilePath", "");
}

