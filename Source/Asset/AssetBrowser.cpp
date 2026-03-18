#include "AssetBrowser.h"
#include "AssetManager.h"
#include "Engine/EditorSelection.h" // ★追加: 選択状態の共有
#include "Graphics.h"
#include "ImGuiRenderer.h"
#include <imgui.h>


void AssetBrowser::Initialize() {
    AssetManager::Instance().Initialize("Data");
    m_currentDirectory = AssetManager::Instance().GetRootDirectory();
}

void AssetBrowser::RenderUI() {
    // 1. タイトルとメニューバー
    ImGui::Begin(ICON_FA_FOLDER_OPEN " Asset Browser", nullptr, ImGuiWindowFlags_MenuBar);

    if (ImGui::BeginMenuBar()) {
        if (ImGui::BeginMenu("View(V)")) { ImGui::EndMenu(); }
        ImGui::EndMenuBar();
    }

    // 2. 検索バーとツールアイコン
    RenderTopBar();

    ImGui::Separator();

    // 3. メインエリアの分割 (左:ツリー, 右:グリッド)
    ImGui::Columns(2, "AssetBrowserSplitter", true);
    if (ImGui::GetColumnWidth() == 0) ImGui::SetColumnWidth(0, 200.0f);

    // --- 左側：フォルダツリー ---
    ImGui::BeginChild("FolderTreeChild", ImVec2(0, 0), false, ImGuiWindowFlags_HorizontalScrollbar);
    std::filesystem::path parentDir = std::filesystem::path(AssetManager::Instance().GetRootDirectory()).parent_path();

    RenderFolderTree(parentDir / "Data");
    RenderFolderTree(parentDir / "Source");
    RenderFolderTree(parentDir / "Shader");

    ImGui::EndChild();

    ImGui::NextColumn();

    // --- 右側：コンテンツグリッド ---
    ImGui::BeginChild("ContentGridChild");
    RenderContentGrid();
    ImGui::EndChild();

    ImGui::Columns(1);
    ImGui::End();
}

// AssetBrowser.cpp の RenderTopBar 内を以下にまるごと差し替え

void AssetBrowser::RenderTopBar() {
    // プロジェクトのルート（Data/Source/Shaderの親）を取得
    std::filesystem::path rootDir = std::filesystem::path(AssetManager::Instance().GetRootDirectory()).parent_path();

    // 戻るボタン
    if (IconFontManager::Instance().IconButton(ICON_FA_ARROW_UP)) {
        if (m_currentDirectory != rootDir && m_currentDirectory.has_parent_path()) {
            m_currentDirectory = m_currentDirectory.parent_path();
        }
    }
    ImGui::SameLine();

    // ==========================================
    // ★ パンくずリスト (Breadcrumb) の生成
    // ==========================================
    std::vector<std::filesystem::path> pathParts;
    std::filesystem::path tempPath = m_currentDirectory;

    // 現在の場所からルートにたどり着くまで遡ってリスト化
    while (tempPath != rootDir && tempPath.has_parent_path()) {
        pathParts.push_back(tempPath);
        tempPath = tempPath.parent_path();
    }

    // ボタンの背景を透明にして文字だけっぽくする
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(1.0f, 1.0f, 1.0f, 0.2f));

    // 親階層から順番にボタンを描画（リストが逆順に入っているので後ろから）
    for (int i = (int)pathParts.size() - 1; i >= 0; --i) {
        std::string folderName = pathParts[i].filename().string();

        if (ImGui::Button(folderName.c_str())) {
            m_currentDirectory = pathParts[i]; // クリックでその階層へワープ！
        }

        // 最後の階層以外は「>」で繋ぐ
        if (i > 0) {
            ImGui::SameLine();
            ImGui::TextDisabled(">");
            ImGui::SameLine();
        }
    }

    ImGui::PopStyleColor(2);

    // ==========================================
    // 検索バー
    // ==========================================
    ImGui::SameLine(ImGui::GetWindowWidth() - 250);
    ImGui::Text("Search");
    ImGui::SameLine();

    char searchBuf[256] = { 0 };
    strncpy_s(searchBuf, sizeof(searchBuf), m_searchFilter.c_str(), _TRUNCATE);

    ImGui::SetNextItemWidth(150.0f);
    if (ImGui::InputText("##Search", searchBuf, sizeof(searchBuf))) {
        m_searchFilter = searchBuf;
    }
}

void AssetBrowser::RenderFolderTree(const std::filesystem::path& currentDir) {
    std::error_code ec;
    if (!std::filesystem::is_directory(currentDir, ec)) return;

    std::string folderName = currentDir.filename().string();
    if (folderName.empty()) folderName = currentDir.string();

    bool isSelected = (m_currentDirectory == currentDir);
    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick | ImGuiTreeNodeFlags_SpanAvailWidth;
    if (isSelected) flags |= ImGuiTreeNodeFlags_Selected;

    std::string pathString = currentDir.string();

    // ==========================================
    // ★ 修正: TreeNodeEx の「直後」でクリック判定を記憶する！
    // ==========================================
    bool isOpen = ImGui::TreeNodeEx(pathString.c_str(), flags, "");
    bool isNodeClicked = ImGui::IsItemClicked(); // ← ここで記憶しておく

    if (ImGui::BeginDragDropTarget()) {
        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ENGINE_ASSET")) {
            std::string sourcePathStr((const char*)payload->Data);
            std::filesystem::path sourcePath(sourcePathStr);

            if (sourcePath.parent_path() != currentDir && sourcePath != currentDir) {
                AssetManager::Instance().MoveAsset(sourcePath, currentDir);
                EditorSelection::Instance().Clear();
            }
        }
        ImGui::EndDragDropTarget();
    }

    // 2. 同じ行にアイコンを描く (Miniサイズを使用)
    ImGui::SameLine();
    ImGui::PushFont(IconFontManager::Instance().GetFontInternal(IconFontSize::Mini));
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.9f, 0.7f, 0.2f, 1.0f)); // フォルダの色
    ImGui::Text(ICON_FA_FOLDER);
    ImGui::PopStyleColor();
    ImGui::PopFont();

    // 3. その横にフォルダ名を書く
    ImGui::SameLine();
    ImGui::Text("%s", folderName.c_str());

    // ==========================================
    // ★ 修正: さっき記憶しておいた判定を使う
    // ==========================================
    if (isNodeClicked) {
        m_currentDirectory = currentDir;
    }

    if (isOpen) {
        auto it = std::filesystem::directory_iterator(currentDir, ec);
        if (!ec) {
            for (; it != std::filesystem::directory_iterator(); it.increment(ec)) {
                if (ec) {
                    ec.clear();
                    continue;
                }
                if (it->is_directory(ec)) {
                    RenderFolderTree(it->path());
                }
            }
        }
        ImGui::TreePop();
    }
}

void AssetBrowser::RenderContentGrid() {
    float cellSize = 100.0f;
    float windowWidth = ImGui::GetContentRegionAvail().x;
    int columnCount = ((int)(windowWidth / cellSize) > 1) ? (int)(windowWidth / cellSize) : 1;

    ImGui::Columns(columnCount, 0, false);

    auto assets = AssetManager::Instance().GetAssetsInDirectory(m_currentDirectory);

    // AssetBrowser.cpp の RenderContentGrid 内

    for (const auto& asset : assets) {
        if (!m_searchFilter.empty() && asset.fileName.find(m_searchFilter) == std::string::npos) continue;

        ImGui::PushID(asset.path.string().c_str());

        // ==========================================
        // 1. 描画の開始位置を記録する
        // ==========================================
        ImVec2 startCursorPos = ImGui::GetCursorPos();

        // --- ビジュアルの描画開始 ---
        ImGui::BeginGroup();

        auto& selection = EditorSelection::Instance();
        bool isSelected = (selection.GetType() == SelectionType::Asset && selection.GetAssetPath() == asset.path.string());
        if (isSelected) {
            ImVec2 p_min = ImGui::GetCursorScreenPos();
            ImVec2 p_max = ImVec2(p_min.x + cellSize, p_min.y + cellSize + ImGui::GetTextLineHeightWithSpacing() * 2);
            ImGui::GetWindowDrawList()->AddRectFilled(p_min, p_max, IM_COL32(50, 100, 200, 128));
        }

        void* thumbnailId = nullptr;
        if (asset.thumbnailTexture) {
            thumbnailId = ImGuiRenderer::GetTextureID(asset.thumbnailTexture.get());
        }


        if (thumbnailId) {
            ImGui::Image(thumbnailId, ImVec2(64, 64));
        }
        else {
            ImGui::PushFont(IconFontManager::Instance().GetFontInternal(IconFontSize::Extra));
            ImGui::PushStyleColor(ImGuiCol_Text, asset.iconColor);
            ImGui::Text("%s", asset.iconStr);
            ImGui::PopStyleColor();
            ImGui::PopFont();
        }

        ImGui::SetNextItemWidth(cellSize);
        ImGui::TextWrapped("%s", asset.fileName.c_str());

        ImGui::EndGroup();
        // --- ビジュアルの描画終了 ---


        // ==========================================
        // ★ 2. 完璧な当たり判定（透明ボタン）を上から被せる
        // ==========================================
        ImVec2 itemSize = ImGui::GetItemRectSize(); // 今描いたアイコン＋文字の大きさを取得

        // カーソルを最初の位置に戻し、同じサイズの「透明なボタン」を配置する
        ImGui::SetCursorPos(startCursorPos);
        ImGui::InvisibleButton("##Interact", itemSize);


        // ==========================================
        // ★ 3. すべての判定はこの透明ボタンに対して行う（絶対にクラッシュしない）
        // ==========================================

        // ① 右クリックメニュー (削除・リネーム)
        if (ImGui::BeginPopupContextItem()) {
            if (ImGui::MenuItem(ICON_FA_SCISSORS " Cut", "Ctrl+X")) {
                m_clipboardPath = asset.path;
                m_isCut = true;
            }
            if (ImGui::MenuItem(ICON_FA_COPY " Copy", "Ctrl+C")) {
                m_clipboardPath = asset.path;
                m_isCut = false;
            }
            ImGui::Separator();
            if (ImGui::MenuItem(ICON_FA_PEN " Rename")) {
                m_renameTarget = asset.path.string();
                strncpy_s(m_renameBuffer, sizeof(m_renameBuffer), asset.fileName.c_str(), _TRUNCATE);
                m_openRenamePopup = true;
            }
            ImGui::Separator();
            if (ImGui::MenuItem(ICON_FA_TRASH " Delete", "Del")) {
                m_deleteTarget = asset.path.string();
                m_openDeletePopup = true;
            }
            ImGui::EndPopup();
        }

        // ② ドラッグ＆ドロップ
        if (asset.type != AssetType::Folder && ImGui::BeginDragDropSource()) {
            std::string pathStr = asset.path.string();
            ImGui::SetDragDropPayload("ENGINE_ASSET", pathStr.c_str(), pathStr.size() + 1);
            ImGui::Text("Placing: %s", asset.fileName.c_str());
            ImGui::EndDragDropSource();
        }

        // ③ 左クリック ＆ ダブルクリック
        if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
            if (asset.type == AssetType::Folder) {
                m_currentDirectory = asset.path; // フォルダなら中に入る
            }
            else {
                AssetManager::Instance().OpenInExternalEditor(asset.path);
            }
        }
        else if (ImGui::IsItemClicked(ImGuiMouseButton_Left)) {
            selection.SelectAsset(asset.path.string()); // アセットを選択
        }

        if (asset.type == AssetType::Folder) {
            if (ImGui::BeginDragDropTarget()) {
                if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ENGINE_ASSET")) {

                    // ペイロードから元のパスを復元
                    std::string sourcePathStr((const char*)payload->Data);
                    std::filesystem::path sourcePath(sourcePathStr);

                    // 自身へのドロップや、同じフォルダへのドロップを防止して移動
                    if (sourcePath.parent_path() != asset.path && sourcePath != asset.path) {
                        AssetManager::Instance().MoveAsset(sourcePath, asset.path);
                        EditorSelection::Instance().Clear(); // 移動したアセットの選択を解除
                    }
                }
                ImGui::EndDragDropTarget();
            }
        }


        ImGui::PopID();
        ImGui::NextColumn();
    }


    ImGui::Columns(1);

    if (ImGui::BeginPopupContextWindow("BrowserEmptySpace", ImGuiPopupFlags_MouseButtonRight | ImGuiPopupFlags_NoOpenOverItems)) {
        if (ImGui::MenuItem(ICON_FA_FOLDER_PLUS " New Folder")) {
            AssetManager::Instance().CreateNewFolder(m_currentDirectory);
        }
        ImGui::Separator();
        if (ImGui::MenuItem(ICON_FA_FILE_CODE " New C++ Script")) {
            AssetManager::Instance().CreateNewScript(m_currentDirectory);
        }
        if (ImGui::MenuItem(ICON_FA_FILE_CODE " New Shader")) {
            AssetManager::Instance().CreateNewShader(m_currentDirectory);
        }
        if (ImGui::MenuItem(ICON_FA_PALETTE " Material")) {
            AssetManager::Instance().CreateNewMaterial(m_currentDirectory);
        }
        ImGui::Separator();

        bool canPaste = !m_clipboardPath.empty() && std::filesystem::exists(m_clipboardPath);
        if (ImGui::MenuItem(ICON_FA_PASTE " Paste", "Ctrl+V", false, canPaste)) {
            if (m_isCut) {
                AssetManager::Instance().MoveAsset(m_clipboardPath, m_currentDirectory);
                m_clipboardPath.clear(); // 切り取りの場合は1回貼ったら記憶を消す
            }
            else {
                AssetManager::Instance().CopyAsset(m_clipboardPath, m_currentDirectory);
            }
        }
        ImGui::EndPopup();
    }

    // ==========================================
    // ★ 追加3: ダイアログの処理 (Rename & Delete)
    // ==========================================
    if (m_openRenamePopup) {
        ImGui::OpenPopup("Rename Asset");
        m_openRenamePopup = false;
    }
    if (ImGui::BeginPopupModal("Rename Asset", NULL, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("Enter new name:");
        ImGui::InputText("##newname", m_renameBuffer, sizeof(m_renameBuffer));
        if (ImGui::Button("OK", ImVec2(120, 0))) {
            AssetManager::Instance().RenameAsset(m_renameTarget, m_renameBuffer);
            ImGui::CloseCurrentPopup();
        }
        ImGui::SetItemDefaultFocus();
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(120, 0))) { ImGui::CloseCurrentPopup(); }
        ImGui::EndPopup();
    }

    if (m_openDeletePopup) {
        ImGui::OpenPopup("Delete Asset");
        m_openDeletePopup = false;
    }
    if (ImGui::BeginPopupModal("Delete Asset", NULL, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("Are you sure you want to delete this?\n\n%s", m_deleteTarget.c_str());
        ImGui::Separator();
        if (ImGui::Button("Yes, Delete", ImVec2(120, 0))) {
            AssetManager::Instance().DeleteAsset(m_deleteTarget);
            EditorSelection::Instance().Clear(); // 消したアセットの選択状態を解除
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(120, 0))) { ImGui::CloseCurrentPopup(); }
        ImGui::EndPopup();
    }

}

