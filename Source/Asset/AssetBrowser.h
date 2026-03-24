#pragma once
#include <string>
#include <filesystem>

class AssetBrowser {
public:
    void Initialize();
    void RenderUI();

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

    std::filesystem::path m_clipboardPath;
    bool m_isCut = false;
};
