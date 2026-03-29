#pragma once
#include <string>
#include <filesystem>

class Registry;

class AssetBrowser {
public:
    void Initialize();
    void RenderUI(bool* p_open = nullptr, bool* outFocused = nullptr);
    void SetRegistry(Registry* registry) { m_registry = registry; }
    bool ConsumePendingSceneLoad(std::filesystem::path& outPath);

    const std::filesystem::path& GetCurrentDirectory() const { return m_currentDirectory; }

    void Refresh() { /* RenderUI Ç≈ç≈êVèÛë‘Çì«Ç›íºÇ∑ */ }
private:
    void RenderTopBar();
    void RenderFolderTree(const std::filesystem::path& currentDir);
    void RenderContentGrid();

    std::filesystem::path m_currentDirectory;
    std::string m_searchFilter;

    std::string m_renameTarget;
    char m_renameBuffer[256] = "";
    bool m_openRenamePopup = false;

    std::string m_deleteTarget;
    bool m_openDeletePopup = false;

    std::string m_loadSceneTarget;
    bool m_openLoadScenePopup = false;

    std::filesystem::path m_clipboardPath;
    bool m_isCut = false;
    Registry* m_registry = nullptr;
    std::filesystem::path m_pendingSceneLoadPath;
};
