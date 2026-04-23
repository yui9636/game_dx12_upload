// AssetManager.cpp

#include "AssetManager.h"

#include <windows.h>
#include <shellapi.h>

#include "ThumbnailGenerator.h"
#include "System/ResourceManager.h"
#include <fstream>
#include "Graphics.h"
#include "RHI/ITexture.h"
#include "RHI/DX11/DX11Texture.h"
#include "Console/Logger.h"

// アセット管理を初期化する。
// ルートディレクトリを保存し、存在しなければ作成する。
void AssetManager::Initialize(const std::string& rootDirectory) {

    // アセットのルートディレクトリを保存する。
    m_rootDirectory = rootDirectory;

    // ルートディレクトリが存在しなければ作成する。
    if (!std::filesystem::exists(m_rootDirectory)) std::filesystem::create_directories(m_rootDirectory);

    // 親ディレクトリを取得する。
    // 現状は未使用だが、将来の拡張用に残してある可能性がある。
    std::filesystem::path parentDir = std::filesystem::path(m_rootDirectory).parent_path();
}

// 指定ディレクトリ内のアセット一覧を取得する。
// フォルダは先頭に、その他は名前順に並べる。
std::vector<AssetEntry> AssetManager::GetAssetsInDirectory(const std::filesystem::path& directory) {

    // 戻り値用の配列。
    std::vector<AssetEntry> entries;

    // std::filesystem 用のエラーコード。
    std::error_code ec;

    // パスが存在しない、またはディレクトリでなければ空配列を返す。
    if (!std::filesystem::exists(directory, ec) || !std::filesystem::is_directory(directory, ec)) return entries;

    // ディレクトリイテレータを作成する。
    auto it = std::filesystem::directory_iterator(directory, ec);
    if (ec) return entries;

    // ディレクトリ内の全エントリを走査する。
    for (; it != std::filesystem::directory_iterator(); it.increment(ec)) {

        // 走査中にエラーが出たら、その要素は飛ばして続行する。
        if (ec) {
            ec.clear();
            continue;
        }

        // 1件分の AssetEntry を作成する。
        AssetEntry asset;

        // ファイルパスを保存する。
        asset.path = it->path();

        // 表示用ファイル名を保存する。
        asset.fileName = it->path().filename().string();

        // 拡張子や種別に応じて icon / type / thumbnail を設定する。
        AssignIconAndType(asset);

        // 一覧へ追加する。
        entries.push_back(asset);
    }

    // フォルダを先に、その後はファイル名でソートする。
    std::sort(entries.begin(), entries.end(), [](const AssetEntry& a, const AssetEntry& b) {
        if (a.type == AssetType::Folder && b.type != AssetType::Folder) return true;
        if (a.type != AssetType::Folder && b.type == AssetType::Folder) return false;
        return a.fileName < b.fileName;
        });

    return entries;
}

// 拡張子やファイル種別に応じて、AssetEntry の種別・アイコン・サムネイルを設定する。
void AssetManager::AssignIconAndType(AssetEntry& entry) {

    // ディレクトリならフォルダ扱いにする。
    if (std::filesystem::is_directory(entry.path)) {

        entry.type = AssetType::Folder;
        entry.iconStr = ICON_FA_FOLDER;
        entry.iconColor = ImVec4(0.9f, 0.7f, 0.2f, 1.0f);
        return;
    }

    // 拡張子を取得し、小文字へ統一する。
    std::string ext = entry.path.extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

    // モデルファイル系。
    if (ext == ".fbx" || ext == ".obj" || ext == ".blend" || ext == ".gltf" || ext == ".cereal") {

        entry.type = AssetType::Model;
        entry.iconStr = ICON_FA_CUBE;
        entry.iconColor = ImVec4(0.4f, 0.8f, 0.9f, 1.0f);

        // モデルのサムネイル生成を要求する。
        ThumbnailGenerator::Instance().Request(entry.path.string());

        // 既に生成済みなら取得する。
        entry.thumbnailTexture = ThumbnailGenerator::Instance().Get(entry.path.string());
    }

    // Prefab / バイナリ / JSON 系。
    else if (ext == ".prefab" || ext == ".bin" || ext == ".json") {

        entry.type = AssetType::Prefab;
        entry.iconStr = ICON_FA_BOXES_STACKED;
        entry.iconColor = ImVec4(0.6f, 0.4f, 0.9f, 1.0f);

        // 現状は prefab 専用サムネイル未対応。
        entry.thumbnail = nullptr;
    }

    // テクスチャ系。
    else if (ext == ".png" || ext == ".jpg" || ext == ".tga" || ext == ".dds" || ext == ".hdr") {

        entry.type = AssetType::Texture;
        entry.iconStr = ICON_FA_IMAGE;
        entry.iconColor = ImVec4(0.5f, 0.9f, 0.5f, 1.0f);

        // ResourceManager からテクスチャを取得する。
        auto tex = ResourceManager::Instance().GetTexture(entry.path.string());
        entry.thumbnailTexture = tex;

        // DX11 の場合はネイティブ SRV を取り出してサムネイル描画に使う。
        if (Graphics::Instance().GetAPI() == GraphicsAPI::DX11 && tex) {
            auto* dx11Texture = dynamic_cast<DX11Texture*>(tex.get());
            entry.thumbnail = dx11Texture ? dx11Texture->GetNativeSRV() : nullptr;
        }
    }

    // フォント系。
    else if (ext == ".ttf" || ext == ".otf" || ext == ".fnt") {

        entry.type = AssetType::Font;
        entry.iconStr = ICON_FA_FONT;
        entry.iconColor = ImVec4(0.95f, 0.85f, 0.45f, 1.0f);
    }

    // 音声系。
    else if (ext == ".wav" || ext == ".ogg" || ext == ".mp3" || ext == ".flac") {

        entry.type = AssetType::Audio;
        entry.iconStr = ICON_FA_VOLUME_HIGH;
        entry.iconColor = ImVec4(0.45f, 0.85f, 1.0f, 1.0f);
    }

    // マテリアル系。
    else if (ext == ".mat") {

        entry.type = AssetType::Material;
        entry.iconStr = ICON_FA_PALETTE;
        entry.iconColor = ImVec4(1.0f, 0.5f, 0.8f, 1.0f);

        // マテリアルサムネイル生成を要求する。
        ThumbnailGenerator::Instance().RequestMaterial(entry.path.string());

        // 既に生成済みなら取得する。
        entry.thumbnailTexture = ThumbnailGenerator::Instance().Get(entry.path.string());
    }

    // コード / シェーダ系。
    else if (ext == ".cpp" || ext == ".h" || ext == ".hlsl" || ext == ".hlsli") {

        entry.type = AssetType::Script;
        entry.iconStr = ICON_FA_FILE_CODE;

        // HLSL 系だけ少し色を変える。
        entry.iconColor = (ext == ".hlsl" || ext == ".hlsli")
            ? ImVec4(0.3f, 0.9f, 0.6f, 1.0f)
            : ImVec4(0.8f, 0.8f, 0.8f, 1.0f);
    }

    // それ以外は unknown 扱い。
    else {

        entry.type = AssetType::Unknown;
        entry.iconStr = ICON_FA_FILE;
        entry.iconColor = ImVec4(0.5f, 0.5f, 0.5f, 1.0f);
    }
}

// OS の既定アプリでファイルを開く。
void AssetManager::OpenInExternalEditor(const std::filesystem::path& path) {

    ShellExecuteA(NULL, "open", path.string().c_str(), NULL, NULL, SW_SHOWNORMAL);
}

// 外部ファイルをアセットディレクトリへコピーして取り込む。
// 同名ファイルがある場合は連番を付けて回避する。
void AssetManager::ImportExternalFile(const std::filesystem::path& sourcePath, const std::filesystem::path& destinationDir) {

    std::error_code ec;

    // コピー先の初期候補を作る。
    std::filesystem::path finalDest = destinationDir / sourcePath.filename();

    // 同名ファイルがある場合は "(1)" "(2)" を付けて回避する。
    int count = 1;
    while (std::filesystem::exists(finalDest, ec)) {
        std::string newName = sourcePath.stem().string() + "(" + std::to_string(count++) + ")" + sourcePath.extension().string();
        finalDest = destinationDir / newName;
    }

    // ファイルをコピーする。
    if (std::filesystem::copy_file(sourcePath, finalDest, std::filesystem::copy_options::none, ec)) {
        // 成功時の追加処理は現状なし。
    }
}

// 新しいフォルダを作成する。
// 既に存在する場合は "NewFolder (1)" のように連番を付ける。
void AssetManager::CreateNewFolder(const std::filesystem::path& parentDir) {

    std::error_code ec;

    // 初期フォルダ名。
    std::filesystem::path newPath = parentDir / "NewFolder";

    // 重複時は連番を付ける。
    int count = 1;
    while (std::filesystem::exists(newPath, ec)) {
        newPath = parentDir / ("NewFolder (" + std::to_string(count++) + ")");
    }

    // フォルダを作成する。
    std::filesystem::create_directory(newPath, ec);
}

// 新しい C++ スクリプトひな型を作成する。
// .h と .cpp をセットで生成する。
void AssetManager::CreateNewScript(const std::filesystem::path& parentDir) {

    std::error_code ec;

    // ベース名を決める。
    std::string baseName = "NewScript";

    // .h と .cpp のパスを作る。
    std::filesystem::path hPath = parentDir / (baseName + ".h");
    std::filesystem::path cppPath = parentDir / (baseName + ".cpp");

    // 既に存在する場合は連番付きへ変更する。
    int count = 1;
    while (std::filesystem::exists(hPath, ec) || std::filesystem::exists(cppPath, ec)) {
        baseName = "NewScript_" + std::to_string(count++);
        hPath = parentDir / (baseName + ".h");
        cppPath = parentDir / (baseName + ".cpp");
    }

    // ヘッダファイルを生成する。
    std::ofstream ofsH(hPath);
    ofsH << "#pragma once\n\n"
        << "class " << baseName << " {\n"
        << "public:\n"
        << "    void Update(float dt);\n"
        << "};\n";
    ofsH.close();

    // CPP ファイルを生成する。
    std::ofstream ofsCpp(cppPath);
    ofsCpp << "#include \"" << baseName << ".h\"\n\n"
        << "void " << baseName << "::Update(float dt) {\n"
        << "      // 実装をここに追加する\\n"
        << "}\\n";
    ofsCpp.close();
}

// 新しい HLSL シェーダひな型を作成する。
void AssetManager::CreateNewShader(const std::filesystem::path& parentDir) {

    std::error_code ec;

    // 初期ファイル名。
    std::filesystem::path newPath = parentDir / "NewShader.hlsl";

    // 重複時は連番を付ける。
    int count = 1;
    while (std::filesystem::exists(newPath, ec)) {
        newPath = parentDir / ("NewShader (" + std::to_string(count++) + ").hlsl");
    }

    // ごく簡単な VS / PS ひな型を書き出す。
    std::ofstream ofs(newPath);
    ofs << "float4 VSMain(float4 pos : POSITION) : SV_POSITION {\n    return pos;\n}\n\nfloat4 PSMain() : SV_TARGET {\n    return float4(1, 1, 1, 1);\n}\n";
    ofs.close();
}

// 新しいマテリアルファイルを作成する。
// JSON 形式の簡易ひな型を書き出す。
void AssetManager::CreateNewMaterial(const std::filesystem::path& parentDir) {

    std::error_code ec;

    // 初期ファイル名。
    std::filesystem::path newPath = parentDir / "NewMaterial.mat";

    // 重複時は連番を付ける。
    int count = 1;
    while (std::filesystem::exists(newPath, ec)) {
        newPath = parentDir / ("NewMaterial (" + std::to_string(count++) + ").mat");
    }

    // ひな型を書き出す。
    std::ofstream ofs(newPath);
    if (ofs.is_open()) {
        ofs << "{\n"
            << "  \"baseColor\": [1.0, 1.0, 1.0, 1.0],\n"
            << "  \"metallic\": 0.0,\n"
            << "  \"roughness\": 0.5,\n"
            << "  \"emissive\": 0.0,\n"
            << "  \"diffuseTexturePath\": \"\",\n"
            << "  \"normalTexturePath\": \"\",\n"
            << "  \"metallicRoughnessTexturePath\": \"\",\n"
            << "  \"emissiveTexturePath\": \"\",\n"
            << "  \"shaderId\": 1,\n"
            << "  \"alphaMode\": 0\n"
            << "}";
        ofs.close();
    }
}

// アセット名を変更する。
// 変更先が既に存在する場合は失敗する。
bool AssetManager::RenameAsset(const std::filesystem::path& oldPath, const std::string& newName) {

    std::error_code ec;

    // 同じ親ディレクトリ内で新しいパスを作る。
    std::filesystem::path newPath = oldPath.parent_path() / newName;

    // 既に存在するなら失敗。
    if (std::filesystem::exists(newPath)) return false;

    // リネームする。
    std::filesystem::rename(oldPath, newPath, ec);

    return !ec;
}

// アセットを削除する。
// フォルダの場合は中身ごと削除する。
void AssetManager::DeleteAsset(const std::filesystem::path& path) {

    std::error_code ec;

    std::filesystem::remove_all(path, ec);
}

// アセットを別ディレクトリへ移動する。
// 同名ファイルがあれば失敗する。
bool AssetManager::MoveAsset(const std::filesystem::path& sourcePath, const std::filesystem::path& destinationDir) {

    std::error_code ec;

    // 移動先パスを作る。
    std::filesystem::path finalDest = destinationDir / sourcePath.filename();

    // 同名ファイルがあれば失敗する。
    if (std::filesystem::exists(finalDest)) {
        return false;
    }

    // リネームによる移動を行う。
    std::filesystem::rename(sourcePath, finalDest, ec);

    return !ec;
}

// アセットを別ディレクトリへコピーする。
// 同名がある場合は " - Copy (n)" を付けて回避する。
bool AssetManager::CopyAsset(const std::filesystem::path& sourcePath, const std::filesystem::path& destinationDir) {

    std::error_code ec;

    // コピー先の初期候補を作る。
    std::filesystem::path finalDest = destinationDir / sourcePath.filename();

    // 同名がある場合は連番付きコピー名へ変える。
    int count = 1;
    while (std::filesystem::exists(finalDest, ec)) {
        std::string newName = sourcePath.stem().string() + " - Copy (" + std::to_string(count++) + ")" + sourcePath.extension().string();
        finalDest = destinationDir / newName;
    }

    // 再帰コピーを行う。
    std::filesystem::copy(sourcePath, finalDest, std::filesystem::copy_options::recursive, ec);

    return !ec;
}