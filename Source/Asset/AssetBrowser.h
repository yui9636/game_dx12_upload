#pragma once
#include <string>
#include <filesystem>

class AssetBrowser {
public:
    void Initialize();
    void RenderUI();

    const std::filesystem::path& GetCurrentDirectory() const { return m_currentDirectory; }

    // ★追加: 表示を強制更新する（D&D後に呼ぶ）
    void Refresh() { /* 次回のRenderUIで自動的に最新のディレクトリが読み込まれます */ }
private:
    void RenderTopBar();
    void RenderFolderTree(const std::filesystem::path& currentDir);
    void RenderContentGrid();

    std::filesystem::path m_currentDirectory; // 現在開いているフォルダ
    std::string m_searchFilter;               // 検索文字列

    // private: に状態変数を追加
    std::string m_renameTarget;
    char m_renameBuffer[256] = "";
    bool m_openRenamePopup = false;

    std::string m_deleteTarget;
    bool m_openDeletePopup = false;

    std::filesystem::path m_clipboardPath; // コピー/切り取り中のファイルパス
    bool m_isCut = false;                  // trueなら切り取り、falseならコピー
};