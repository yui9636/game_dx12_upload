#include "GameLoopScenePicker.h"

#include "Asset/AssetManager.h"
#include "GameLoopAsset.h"

#include <algorithm>
#include <cctype>
#include <cstring>
#include <system_error>

#include <imgui.h>

namespace
{
    constexpr const char* kEngineAssetPayloadName = "ENGINE_ASSET";
    constexpr float kPickerWidth = 560.0f;
    constexpr float kPickerHeight = 420.0f;

    float MaxFloat(float a, float b)
    {
        return a > b ? a : b;
    }
}

void GameLoopScenePicker::Open(const char* popupId)
{
    (void)popupId;
    m_openRequested = true;
    m_needsRefresh = true;
}

bool GameLoopScenePicker::Draw(const char* popupId, std::string& outScenePath)
{
    if (m_openRequested) {
        ImGui::OpenPopup(popupId);
        m_openRequested = false;
    }

    ImGui::SetNextWindowSize(ImVec2(kPickerWidth, kPickerHeight), ImGuiCond_FirstUseEver);

    bool selected = false;
    if (ImGui::BeginPopupModal(popupId, nullptr, ImGuiWindowFlags_NoSavedSettings)) {
        if (m_needsRefresh) {
            RefreshSceneList();
            m_needsRefresh = false;
        }

        ImGui::TextUnformatted("Select Scene");
        ImGui::Separator();

        ImGui::SetNextItemWidth(-92.0f);
        ImGui::InputTextWithHint("##ScenePickerSearch", "Search scene...", m_searchBuffer, sizeof(m_searchBuffer));
        ImGui::SameLine();
        if (ImGui::Button("Refresh")) {
            RequestRefresh();
        }

        ImGui::Separator();

        ImVec2 listSize = ImGui::GetContentRegionAvail();
        listSize.y -= ImGui::GetFrameHeightWithSpacing() + 8.0f;
        listSize.y = MaxFloat(listSize.y, 80.0f);

        if (ImGui::BeginChild("##ScenePickerList", listSize, true, ImGuiWindowFlags_AlwaysVerticalScrollbar)) {
            bool hasVisibleEntry = false;

            for (const SceneEntry& entry : m_sceneEntries) {
                if (!PassesSearchFilter(entry)) {
                    continue;
                }

                hasVisibleEntry = true;
                if (DrawSceneEntry(entry, outScenePath)) {
                    selected = true;
                    ImGui::CloseCurrentPopup();
                    break;
                }
            }

            if (!hasVisibleEntry) {
                ImGui::TextDisabled("No scene files found.");
            }
        }
        ImGui::EndChild();

        ImGui::Separator();
        if (ImGui::Button("Cancel")) {
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }

    return selected;
}

void GameLoopScenePicker::RequestRefresh()
{
    m_needsRefresh = true;
}

bool GameLoopScenePicker::AcceptSceneAssetDragDrop(std::string& outScenePath) const
{
    bool accepted = false;

    if (ImGui::BeginDragDropTarget()) {
        const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(kEngineAssetPayloadName);
        if (payload && payload->Data && payload->DataSize > 0) {
            const char* payloadPath = static_cast<const char*>(payload->Data);
            const std::filesystem::path rawPath(payloadPath);

            if (IsSceneAssetPath(rawPath)) {
                const std::string normalized = NormalizeScenePath(rawPath);
                if (!normalized.empty()) {
                    outScenePath = normalized;
                    accepted = true;
                }
            }
        }
        ImGui::EndDragDropTarget();
    }

    return accepted;
}

bool GameLoopScenePicker::IsSceneAssetPath(const std::filesystem::path& path)
{
    const std::string filename = path.filename().string();
    return EndsWithCaseInsensitive(filename, ".scene") ||
        EndsWithCaseInsensitive(filename, ".scene.json");
}

std::string GameLoopScenePicker::NormalizeScenePath(const std::filesystem::path& path)
{
    if (!IsSceneAssetPath(path)) {
        return {};
    }

    const std::string normalized = NormalizeGameLoopScenePath(path.string());
    if (normalized.empty()) {
        return {};
    }

    if (!IsSceneAssetPath(std::filesystem::path(normalized))) {
        return {};
    }

    return normalized;
}

std::string GameLoopScenePicker::BuildNodeNameFromScenePath(const std::string& scenePath)
{
    std::string filename = std::filesystem::path(scenePath).filename().string();

    if (EndsWithCaseInsensitive(filename, ".scene.json")) {
        filename.resize(filename.size() - std::strlen(".scene.json"));
        return filename.empty() ? std::string("Scene") : filename;
    }

    if (EndsWithCaseInsensitive(filename, ".scene")) {
        filename.resize(filename.size() - std::strlen(".scene"));
        return filename.empty() ? std::string("Scene") : filename;
    }

    const std::string stem = std::filesystem::path(scenePath).stem().string();
    return stem.empty() ? std::string("Scene") : stem;
}

std::filesystem::path GameLoopScenePicker::GetDataRootDirectory() const
{
    std::filesystem::path dataRoot = AssetManager::Instance().GetRootDirectory();
    if (dataRoot.empty()) {
        dataRoot = "Data";
    }
    return dataRoot;
}

void GameLoopScenePicker::RefreshSceneList()
{
    m_sceneEntries.clear();

    const std::filesystem::path dataRoot = GetDataRootDirectory();

    std::error_code ec;
    if (!std::filesystem::exists(dataRoot, ec) || !std::filesystem::is_directory(dataRoot, ec)) {
        return;
    }

    const std::filesystem::directory_options options =
        std::filesystem::directory_options::skip_permission_denied;

    std::filesystem::recursive_directory_iterator it(dataRoot, options, ec);
    const std::filesystem::recursive_directory_iterator end;
    if (ec) {
        return;
    }

    for (; it != end; it.increment(ec)) {
        if (ec) {
            ec.clear();
            continue;
        }

        std::error_code fileEc;
        if (!it->is_regular_file(fileEc)) {
            continue;
        }

        const std::filesystem::path assetPath = it->path();
        if (!IsSceneAssetPath(assetPath)) {
            continue;
        }

        const std::string scenePath = NormalizeScenePath(assetPath);
        if (scenePath.empty()) {
            continue;
        }

        SceneEntry entry;
        entry.scenePath = scenePath;
        entry.fileName = assetPath.filename().string();
        entry.folderPath = std::filesystem::path(scenePath).parent_path().generic_string();
        entry.searchText = ToLowerCopy(entry.scenePath);
        m_sceneEntries.push_back(entry);
    }

    std::sort(
        m_sceneEntries.begin(),
        m_sceneEntries.end(),
        [](const SceneEntry& a, const SceneEntry& b) {
            return a.scenePath < b.scenePath;
        });
}

bool GameLoopScenePicker::PassesSearchFilter(const SceneEntry& entry) const
{
    if (m_searchBuffer[0] == '\0') {
        return true;
    }

    const std::string search = ToLowerCopy(m_searchBuffer);
    return entry.searchText.find(search) != std::string::npos;
}

bool GameLoopScenePicker::DrawSceneEntry(const SceneEntry& entry, std::string& outScenePath)
{
    ImGui::PushID(entry.scenePath.c_str());

    bool selected = false;

    ImGui::BeginGroup();
    ImGui::TextUnformatted(entry.fileName.c_str());
    ImGui::TextDisabled("%s", entry.folderPath.c_str());
    ImGui::EndGroup();

    const ImVec2 itemMin = ImGui::GetItemRectMin();
    const ImVec2 itemSize = ImGui::GetItemRectSize();
    ImGui::SetCursorScreenPos(itemMin);

    if (ImGui::InvisibleButton("##SceneEntry", itemSize)) {
        outScenePath = entry.scenePath;
        selected = true;
    }

    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("%s", entry.scenePath.c_str());
    }

    ImGui::PopID();
    return selected;
}

std::string GameLoopScenePicker::ToLowerCopy(const std::string& value)
{
    std::string result = value;
    for (char& c : result) {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    return result;
}

bool GameLoopScenePicker::EndsWithCaseInsensitive(const std::string& value, const std::string& suffix)
{
    if (value.size() < suffix.size()) {
        return false;
    }

    const size_t start = value.size() - suffix.size();
    for (size_t i = 0; i < suffix.size(); ++i) {
        const char lhs = static_cast<char>(std::tolower(static_cast<unsigned char>(value[start + i])));
        const char rhs = static_cast<char>(std::tolower(static_cast<unsigned char>(suffix[i])));
        if (lhs != rhs) {
            return false;
        }
    }
    return true;
}
