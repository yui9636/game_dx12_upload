#pragma once

#include <filesystem>
#include <string>
#include <vector>
// GameLoopScenePicker は SceneEntry/scenePath/fileName/folderPath を中心に、実行時やエディターで共有する状態を保持する。

class GameLoopScenePicker
{
public:
    void Open(const char* popupId);

    bool Draw(const char* popupId, std::string& outScenePath);

    void RequestRefresh();

    bool AcceptSceneAssetDragDrop(std::string& outScenePath) const;

    static bool IsSceneAssetPath(const std::filesystem::path& path);
    static std::string NormalizeScenePath(const std::filesystem::path& path);
    static std::string BuildNodeNameFromScenePath(const std::string& scenePath);

private:
    struct SceneEntry
    {
        std::string scenePath;
        std::string fileName;
        std::string folderPath;
        std::string searchText;
    };

private:
    std::filesystem::path GetDataRootDirectory() const;

    void RefreshSceneList();

    bool PassesSearchFilter(const SceneEntry& entry) const;

    bool DrawSceneEntry(const SceneEntry& entry, std::string& outScenePath);

    static std::string ToLowerCopy(const std::string& value);

    static bool EndsWithCaseInsensitive(const std::string& value, const std::string& suffix);

private:
    std::vector<SceneEntry> m_sceneEntries;
    bool m_openRequested = false;
    bool m_needsRefresh = true;
    char m_searchBuffer[128] = {};
};
