#include "Storage/GEStorageCompilerComponent.h"
#include "JSONManager.h"
#include "System/Dialog.h"      
#include "imgui.h"
#include <filesystem>
#include <fstream>

namespace fs = std::filesystem;

void GEStorageCompilerComponent::SaveGameplayData(const std::string& modelName,
    const std::vector<std::vector<GESequencerItem>>& timelines,
    const std::vector<GECurveSettings>& curves)
{
    if (modelName.empty()) return;
    SaveGameplayDataToPath(GetFilePath(modelName), timelines, curves);
}

void GEStorageCompilerComponent::SaveGameplayDataToPath(const std::string& fullPath,
    const std::vector<std::vector<GESequencerItem>>& timelines,
    const std::vector<GECurveSettings>& curves)
{
    if (fullPath.empty()) return;

    GameplayAsset asset;
    asset.timelines = timelines;
    asset.curves = curves;

    fs::path dir = fs::path(fullPath).parent_path();
    if (!fs::exists(dir) && !dir.empty()) {
        fs::create_directories(dir);
    }

    JSONManager json(fullPath);
    json.Set("gameplayAsset", asset);
    json.Save();
}

GameplayAsset GEStorageCompilerComponent::LoadGameplayData(const std::string& modelName)
{
    if (modelName.empty()) return {};
    return LoadGameplayDataFromPath(GetFilePath(modelName));
}

GameplayAsset GEStorageCompilerComponent::LoadGameplayDataFromPath(const std::string& fullPath)
{
    if (fullPath.empty() || !fs::exists(fullPath)) return {};

    JSONManager json(fullPath);
    return json.Get<GameplayAsset>("gameplayAsset", GameplayAsset{});
}

std::string GEStorageCompilerComponent::GetFilePath(const std::string& modelName) const
{
    return "Data/Gameplay/" + modelName + "_Gameplay.json";
}

void GEStorageCompilerComponent::OnGUI()
{
    ImGui::TextDisabled("Gameplay Data Source");

    char buf[256];
    strcpy_s(buf, targetFilePath.c_str());
    if (ImGui::InputText("Path", buf, sizeof(buf))) {
        targetFilePath = buf;
    }

    ImGui::SameLine();

    if (ImGui::Button("..."))
    {
        char filepath[MAX_PATH] = "";
        if (Dialog::OpenFileName(filepath, MAX_PATH, "Gameplay JSON\0*.json\0All Files\0*.*\0") == DialogResult::OK)
        {
            targetFilePath = filepath;
        }
    }

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
    outJson["targetFilePath"] = targetFilePath;
}

void GEStorageCompilerComponent::Deserialize(const json& inJson)
{
    targetFilePath = inJson.value("targetFilePath", "");
}

