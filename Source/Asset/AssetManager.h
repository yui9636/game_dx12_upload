// AssetManager.h

#pragma once

#include <string>
#include <vector>
#include <memory>
#include <filesystem>
#include <imgui.h>
#include <d3d11.h>
#include <wrl/client.h>

#include "Icon/IconFontManager.h" // アイコン文字列定義などで使う

class ITexture;

// アセットの種類を表す列挙。
// Asset Browser 上での表示やアイコン切り替えに使う。
enum class AssetType {
    Folder,
    Model,
    Texture,
    Font,
    Prefab,
    Script,
    Audio,
    Material,
    Unknown
};

// Asset Browser に表示する 1 件ぶんの情報をまとめた構造体。
struct AssetEntry {

    // 表示用のファイル名。
    std::string fileName;

    // 実際のファイルパス。
    std::filesystem::path path;

    // アセットの種類。
    AssetType type;

    // DX11 用のサムネイル SRV。
    // 既存の ImGui 表示などで直接使うためのもの。
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> thumbnail;

    // API 非依存寄りのテクスチャ参照。
    // DX11 以外の描画API対応や管理のために持つ。
    std::shared_ptr<ITexture> thumbnailTexture;

    // 表示する FontAwesome 系アイコン文字列。
    const char* iconStr = nullptr;

    // アイコンの表示色。
    ImVec4 iconColor = { 1, 1, 1, 1 };
};

// アセットブラウザ用のアセット管理クラス。
// ディレクトリ列挙、種別判定、サムネイル設定、ファイル作成・削除・移動などを担当する。
class AssetManager {
public:

    // singleton インスタンスを返す。
    static AssetManager& Instance() { static AssetManager instance; return instance; }

    // ルートディレクトリを設定して初期化する。
    void Initialize(const std::string& rootDirectory);

    // 指定ディレクトリ内のアセット一覧を取得する。
    std::vector<AssetEntry> GetAssetsInDirectory(const std::filesystem::path& directory);

    // OS の既定アプリでアセットを開く。
    void OpenInExternalEditor(const std::filesystem::path& path);

    // 現在のアセットルートディレクトリを返す。
    const std::filesystem::path& GetRootDirectory() const { return m_rootDirectory; }

    // 外部ファイルをアセットディレクトリへ取り込む。
    void ImportExternalFile(const std::filesystem::path& sourcePath, const std::filesystem::path& destinationDir);

    // 新規フォルダを作成する。
    void CreateNewFolder(const std::filesystem::path& parentDir);

    // 新規 C++ スクリプトひな型を作成する。
    void CreateNewScript(const std::filesystem::path& parentDir);

    // 新規シェーダひな型を作成する。
    void CreateNewShader(const std::filesystem::path& parentDir);

    // 新規マテリアルひな型を作成する。
    void CreateNewMaterial(const std::filesystem::path& parentDir);

    // アセット名を変更する。
    // 成功時は true、失敗時は false を返す。
    bool RenameAsset(const std::filesystem::path& oldPath, const std::string& newName);

    // アセットをコピーする。
    // 成功時は true、失敗時は false を返す。
    bool CopyAsset(const std::filesystem::path& sourcePath, const std::filesystem::path& destinationDir);

    // アセットを移動する。
    // 成功時は true、失敗時は false を返す。
    bool MoveAsset(const std::filesystem::path& sourcePath, const std::filesystem::path& destinationDir);

    // アセットを削除する。
    void DeleteAsset(const std::filesystem::path& path);

private:

    // singleton 用なのでコンストラクタは private。
    AssetManager() = default;

    // アセットのルートディレクトリ。
    std::filesystem::path m_rootDirectory;

    // 拡張子や種別に応じて AssetEntry のアイコン・色・種類・サムネイルを設定する。
    void AssignIconAndType(AssetEntry& entry);
};