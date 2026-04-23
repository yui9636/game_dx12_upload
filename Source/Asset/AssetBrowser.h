#pragma once
#include <string>
#include <filesystem>

class Registry;

// アセットブラウザUIを管理するクラス。
// フォルダツリー表示、コンテンツグリッド表示、検索、リネーム、削除、シーンロード要求などを担当する。
class AssetBrowser {
public:
    // AssetBrowser を初期化する。
    // ルートディレクトリ設定や現在ディレクトリ初期化を行う。
    void Initialize();

    // AssetBrowser の UI 全体を描画する。
    // p_open が指定されていればウィンドウ開閉状態を受け取り、
    // outFocused が指定されていればフォーカス状態を返す。
    void RenderUI(bool* p_open = nullptr, bool* outFocused = nullptr);

    // Entity から prefab 保存などを行うために Registry を設定する。
    void SetRegistry(Registry* registry) { m_registry = registry; }

    // 保留中のシーンロード要求を取り出す。
    // 要求があれば outPath に書き込み、内部状態をクリアして true を返す。
    bool ConsumePendingSceneLoad(std::filesystem::path& outPath);

    // 現在表示中のディレクトリを返す。
    const std::filesystem::path& GetCurrentDirectory() const { return m_currentDirectory; }

    // AssetBrowser の内容を更新する。
    // 実際の再読込は RenderUI 側で自然に行う前提。
    void Refresh() { /* RenderUI で最新状態を読み直す */ }

private:
    // 上部バーを描画する。
    // パンくず、検索、親ディレクトリ移動などを担当する。
    void RenderTopBar();

    // フォルダツリーを再帰的に描画する。
    void RenderFolderTree(const std::filesystem::path& currentDir);

    // 現在ディレクトリのコンテンツをグリッド表示する。
    void RenderContentGrid();

    // 現在表示中のディレクトリ。
    std::filesystem::path m_currentDirectory;

    // 検索フィルタ文字列。
    std::string m_searchFilter;

    // リネーム対象のフルパス文字列。
    std::string m_renameTarget;

    // リネーム入力用バッファ。
    char m_renameBuffer[256] = "";

    // 次フレームでリネームポップアップを開くかどうか。
    bool m_openRenamePopup = false;

    // 削除対象のフルパス文字列。
    std::string m_deleteTarget;

    // 次フレームで削除確認ポップアップを開くかどうか。
    bool m_openDeletePopup = false;

    // ロード対象のシーンパス文字列。
    std::string m_loadSceneTarget;

    // 次フレームでシーンロード確認ポップアップを開くかどうか。
    bool m_openLoadScenePopup = false;

    // コピー/カット操作で保持しているアセットパス。
    std::filesystem::path m_clipboardPath;

    // クリップボード内容が Cut かどうか。
    // false の場合は Copy 扱い。
    bool m_isCut = false;

    // Entity から prefab 保存などを行うための Registry 参照。
    Registry* m_registry = nullptr;

    // シーンロード要求を保留しておくパス。
    // 実際のロードは外部側で ConsumePendingSceneLoad を使って行う。
    std::filesystem::path m_pendingSceneLoadPath;
};