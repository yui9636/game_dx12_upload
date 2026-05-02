#include "AssetBrowser.h"
#include "AssetManager.h"
#include "PrefabSystem.h"
#include "Registry/Registry.h"
#include "ThumbnailGenerator.h"
#include "Engine/EditorSelection.h"
#include "Engine/EditorAssetContext.h"
#include "Engine/Editor2DEntityUtils.h"
#include "Engine/EngineKernel.h"
#include "Component/CanvasItemComponent.h"
#include "Component/HierarchyComponent.h"
#include "Component/NameComponent.h"
#include "Component/RectTransformComponent.h"
#include "Component/SpriteComponent.h"
#include "Component/TransformComponent.h"
#include "Component/UIButtonComponent.h"
#include "System/ResourceManager.h"
#include "System/UndoSystem.h"
#include "Undo/ComponentUndoAction.h"
#include "Undo/EntitySnapshot.h"
#include "Undo/EntityUndoActions.h"
#include "Graphics.h"
#include "ImGuiRenderer.h"
#include <unordered_set>
#include <cstring>
#include <optional>
#include <utility>
#include <imgui.h>

namespace
{
    // 指定パスがシーンアセットかどうかを判定する。
    // 新形式の .scene と、旧形式の .scene.json の両方を許可する。
    bool IsSceneAssetPath(const std::filesystem::path& path)
    {
        const std::string filename = path.filename().string();
        const char* legacyExtension = ".scene.json";
        const size_t legacyLength = std::strlen(legacyExtension);

        // 旧形式の ".scene.json" かどうかを判定する。
        const bool isLegacyScene = filename.size() >= legacyLength &&
            filename.compare(filename.size() - legacyLength, legacyLength, legacyExtension) == 0;

        // 新旧どちらかの拡張子ならシーン扱いとする。
        return path.extension() == ".scene" || isLegacyScene;
    }

    bool IsTextureAssetType(AssetType type)
    {
        return type == AssetType::Texture;
    }

    EntitySnapshot::Snapshot BuildSpriteSnapshotFromTexture(const std::filesystem::path& path, bool asButton)
    {
        EntitySnapshot::Snapshot snapshot;
        snapshot.rootLocalID = 0;

        EntitySnapshot::Node node;
        node.localID = 0;
        node.sourceEntity = Entity::NULL_ID;
        node.parentLocalID = EntitySnapshot::kInvalidLocalID;
        node.externalParent = Entity::NULL_ID;

        const std::string texturePath = path.string();
        const std::string stem = path.stem().string();
        const std::string name = stem.empty() ? (asButton ? "Button" : "Sprite") : stem;

        DirectX::XMFLOAT2 size = asButton
            ? DirectX::XMFLOAT2{ 180.0f, 64.0f }
            : DirectX::XMFLOAT2{ 128.0f, 128.0f };
        if (auto texture = ResourceManager::Instance().GetTexture(texturePath)) {
            size = {
                static_cast<float>(texture->GetWidth()),
                static_cast<float>(texture->GetHeight())
            };
        }

        TransformComponent transform{};
        transform.localScale = { 1.0f, 1.0f, 1.0f };
        transform.isDirty = true;

        RectTransformComponent rect{};
        rect.sizeDelta = size;

        CanvasItemComponent canvas{};

        SpriteComponent sprite{};
        sprite.textureAssetPath = texturePath;

        std::get<std::optional<NameComponent>>(node.components) = NameComponent{ name };
        std::get<std::optional<TransformComponent>>(node.components) = transform;
        std::get<std::optional<HierarchyComponent>>(node.components) = HierarchyComponent{};
        std::get<std::optional<RectTransformComponent>>(node.components) = rect;
        std::get<std::optional<CanvasItemComponent>>(node.components) = canvas;
        std::get<std::optional<SpriteComponent>>(node.components) = sprite;

        if (asButton) {
            UIButtonComponent button{};
            button.buttonId = name;
            button.enabled = true;
            std::get<std::optional<UIButtonComponent>>(node.components) = button;
        }

        snapshot.nodes.push_back(std::move(node));
        return snapshot;
    }

    void CreateSpriteLikeEntityFromTexture(Registry* registry,
                                           const std::filesystem::path& path,
                                           bool asButton)
    {
        if (!registry || !IsTextureAssetType(EditorAssetContext::GuessAssetType(path.string()))) {
            return;
        }

        auto action = std::make_unique<CreateEntityAction>(
            BuildSpriteSnapshotFromTexture(path, asButton),
            Entity::NULL_ID,
            asButton ? "Create UI Button From Texture" : "Create Sprite From Texture");
        auto* actionPtr = action.get();
        UndoSystem::Instance().ExecuteAction(std::move(action), *registry);

        const EntityID liveRoot = actionPtr->GetLiveRoot();
        if (!Entity::IsNull(liveRoot)) {
            Editor2D::FinalizeCreatedEntity(*registry, liveRoot);
            EditorSelection::Instance().SelectEntity(liveRoot);
        }
    }

    void AssignTextureToSelectedSprite(Registry* registry, const std::filesystem::path& path)
    {
        if (!registry || !IsTextureAssetType(EditorAssetContext::GuessAssetType(path.string()))) {
            return;
        }

        const EntityID entity = EditorSelection::Instance().GetPrimaryEntity();
        if (Entity::IsNull(entity) || !registry->IsAlive(entity)) {
            return;
        }

        auto* sprite = registry->GetComponent<SpriteComponent>(entity);
        if (!sprite) {
            return;
        }

        const SpriteComponent before = *sprite;
        sprite->textureAssetPath = path.string();
        UndoSystem::Instance().ExecuteAction(
            std::make_unique<ComponentUndoAction<SpriteComponent>>(entity, before, *sprite),
            *registry);
        Editor2D::FinalizeCreatedEntity(*registry, entity);
        PrefabSystem::MarkPrefabOverride(entity, *registry);
        EditorSelection::Instance().SelectEntity(entity);
    }
}

// 保留中のシーンロード要求を取り出す。
// 要求があれば outPath に書き込み、内部状態をクリアして true を返す。
bool AssetBrowser::ConsumePendingSceneLoad(std::filesystem::path& outPath)
{
    // 保留中パスが無ければ false を返す。
    if (m_pendingSceneLoadPath.empty()) {
        return false;
    }

    // 呼び出し元へパスを返す。
    outPath = m_pendingSceneLoadPath;

    // 一度消費したので内部状態をクリアする。
    m_pendingSceneLoadPath.clear();
    return true;
}

// AssetBrowser を初期化する。
// AssetManager を初期化し、現在ディレクトリをルートへ合わせる。
void AssetBrowser::Initialize() {
    AssetManager::Instance().Initialize("Data");
    m_currentDirectory = AssetManager::Instance().GetRootDirectory();
}

// AssetBrowser の UI 全体を描画する。
void AssetBrowser::RenderUI(bool* p_open, bool* outFocused) {
    // ウィンドウを開く。折りたたまれている場合は描画せず終了する。
    if (!ImGui::Begin(ICON_FA_FOLDER_OPEN " Asset Browser", p_open, ImGuiWindowFlags_MenuBar)) {
        if (outFocused) {
            *outFocused = false;
        }
        ImGui::End();
        return;
    }

    // 現在ウィンドウがフォーカスされているかを返す。
    if (outFocused) {
        *outFocused = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);
    }

    // メニューバーを描画する。
    if (ImGui::BeginMenuBar()) {
        if (ImGui::BeginMenu("View(V)")) {
            ImGui::EndMenu();
        }
        ImGui::EndMenuBar();
    }

    // 上部バーを描画する。
    RenderTopBar();

    ImGui::Separator();

    // 左右2カラム構成にする。
    ImGui::Columns(2, "AssetBrowserSplitter", true);

    // 左カラム幅が未設定なら初期幅を与える。
    if (ImGui::GetColumnWidth() == 0) {
        ImGui::SetColumnWidth(0, 200.0f);
    }

    // 左カラム: フォルダツリー。
    ImGui::BeginChild("FolderTreeChild", ImVec2(0, 0), false, ImGuiWindowFlags_HorizontalScrollbar);

    std::filesystem::path parentDir = std::filesystem::path(AssetManager::Instance().GetRootDirectory()).parent_path();

    // ルート候補を順に表示する。
    RenderFolderTree(parentDir / "Data");
    RenderFolderTree(parentDir / "Source");
    RenderFolderTree(parentDir / "Shader");

    ImGui::EndChild();

    // 右カラムへ移動する。
    ImGui::NextColumn();

    // 右カラム: コンテンツグリッド。
    ImGui::BeginChild("ContentGridChild");
    RenderContentGrid();
    ImGui::EndChild();

    // カラム状態を戻す。
    ImGui::Columns(1);
    ImGui::End();
}

// 上部バーを描画する。
// 親フォルダ移動、パンくず表示、検索欄を担当する。
void AssetBrowser::RenderTopBar() {
    std::filesystem::path rootDir = std::filesystem::path(AssetManager::Instance().GetRootDirectory()).parent_path();

    // 親フォルダへ戻るボタン。
    if (IconFontManager::Instance().IconButton(ICON_FA_ARROW_UP)) {
        if (m_currentDirectory != rootDir && m_currentDirectory.has_parent_path()) {
            m_currentDirectory = m_currentDirectory.parent_path();
        }
    }
    ImGui::SameLine();

    // 現在ディレクトリのパンくずリストを作る。
    std::vector<std::filesystem::path> pathParts;
    std::filesystem::path tempPath = m_currentDirectory;

    while (tempPath != rootDir && tempPath.has_parent_path()) {
        pathParts.push_back(tempPath);
        tempPath = tempPath.parent_path();
    }

    // パンくずボタンの見た目を少し薄くする。
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(1.0f, 1.0f, 1.0f, 0.2f));

    // ルート側から順番にパンくずを表示する。
    for (int i = (int)pathParts.size() - 1; i >= 0; --i) {
        std::string folderName = pathParts[i].filename().string();

        // パンくずを押したらその階層へ移動する。
        if (ImGui::Button(folderName.c_str())) {
            m_currentDirectory = pathParts[i];
        }

        // 区切り文字を表示する。
        if (i > 0) {
            ImGui::SameLine();
            ImGui::TextDisabled(">");
            ImGui::SameLine();
        }
    }

    ImGui::PopStyleColor(2);

    // 右側へ検索UIを寄せる。
    ImGui::SameLine(ImGui::GetWindowWidth() - 250);
    ImGui::Text("Search");
    ImGui::SameLine();

    // 検索文字列を編集用バッファへコピーする。
    char searchBuf[256] = { 0 };
    strncpy_s(searchBuf, sizeof(searchBuf), m_searchFilter.c_str(), _TRUNCATE);

    // 検索入力欄を描画する。
    ImGui::SetNextItemWidth(150.0f);
    if (ImGui::InputText("##Search", searchBuf, sizeof(searchBuf))) {
        m_searchFilter = searchBuf;
    }
}

// フォルダツリーを再帰的に描画する。
void AssetBrowser::RenderFolderTree(const std::filesystem::path& currentDir) {
    std::error_code ec;

    // ディレクトリでなければ描画しない。
    if (!std::filesystem::is_directory(currentDir, ec)) return;

    // 表示名を決める。
    std::string folderName = currentDir.filename().string();
    if (folderName.empty()) folderName = currentDir.string();

    // 現在選択中のフォルダか判定する。
    bool isSelected = (m_currentDirectory == currentDir);

    // ツリーノードの表示フラグを設定する。
    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick | ImGuiTreeNodeFlags_SpanAvailWidth;
    if (isSelected) flags |= ImGuiTreeNodeFlags_Selected;

    std::string pathString = currentDir.string();

    // ツリーノード本体を描画する。
    bool isOpen = ImGui::TreeNodeEx(pathString.c_str(), flags, "");
    bool isNodeClicked = ImGui::IsItemClicked();

    // フォルダノードへのドラッグ&ドロップ受け入れ。
    if (ImGui::BeginDragDropTarget()) {
        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ENGINE_ASSET")) {
            std::string sourcePathStr((const char*)payload->Data);
            std::filesystem::path sourcePath(sourcePathStr);

            // 同一親や自分自身への移動は防ぐ。
            if (sourcePath.parent_path() != currentDir && sourcePath != currentDir) {
                AssetManager::Instance().MoveAsset(sourcePath, currentDir);
                EditorSelection::Instance().Clear();
            }
        }
        ImGui::EndDragDropTarget();
    }

    // フォルダアイコンを表示する。
    ImGui::SameLine();
    ImGui::PushFont(IconFontManager::Instance().GetFontInternal(IconFontSize::Mini));
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.9f, 0.7f, 0.2f, 1.0f));
    ImGui::Text(ICON_FA_FOLDER);
    ImGui::PopStyleColor();
    ImGui::PopFont();

    // フォルダ名を表示する。
    ImGui::SameLine();
    ImGui::Text("%s", folderName.c_str());

    // ノードをクリックしたら現在ディレクトリを切り替える。
    if (isNodeClicked) {
        m_currentDirectory = currentDir;
    }

    // ノードが開いているなら子フォルダを再帰描画する。
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

// 現在ディレクトリ内のアセットをグリッド表示する。
void AssetBrowser::RenderContentGrid() {
    // 1セルの幅を決める。
    float cellSize = 100.0f;

    // 利用可能幅から列数を決める。
    float windowWidth = ImGui::GetContentRegionAvail().x;
    int columnCount = ((int)(windowWidth / cellSize) > 1) ? (int)(windowWidth / cellSize) : 1;

    // 現在可視状態のモデル/マテリアルパスを集める。
    std::unordered_set<std::string> visiblePaths;

    ImGui::Columns(columnCount, 0, false);

    // 現在ディレクトリのアセット一覧を取得する。
    auto assets = AssetManager::Instance().GetAssetsInDirectory(m_currentDirectory);

    for (auto& asset : assets) {
        // 検索フィルタがある場合は一致しないものを飛ばす。
        if (!m_searchFilter.empty() && asset.fileName.find(m_searchFilter) == std::string::npos) continue;

        // モデル/マテリアルは可視パスとして記録する。
        if (asset.type == AssetType::Model || asset.type == AssetType::Material) {
            visiblePaths.insert(asset.path.string());
        }

        ImGui::PushID(asset.path.string().c_str());

        // 現在の描画開始位置を保存する。
        ImVec2 startCursorPos = ImGui::GetCursorPos();

        ImGui::BeginGroup();

        // 選択状態を確認する。
        auto& selection = EditorSelection::Instance();
        EditorAssetContext& assetContext = EditorAssetContext::Instance();
        const bool isAssetSelection = selection.GetType() == SelectionType::Asset && selection.GetAssetPath() == asset.path.string();
        const bool isActiveAsset = assetContext.GetActiveAssetPath() == asset.path.string();
        bool isSelected = isAssetSelection || isActiveAsset;

        // 選択中なら背景ハイライトを描画する。
        if (isSelected) {
            ImVec2 p_min = ImGui::GetCursorScreenPos();
            ImVec2 p_max = ImVec2(p_min.x + cellSize, p_min.y + cellSize + ImGui::GetTextLineHeightWithSpacing() * 2);
            ImGui::GetWindowDrawList()->AddRectFilled(p_min, p_max, IM_COL32(50, 100, 200, 128));
        }

        // サムネイル表示用 ID を用意する。
        void* thumbnailId = nullptr;

        // モデル/マテリアルでまだサムネイル未取得なら再取得を試みる。
        if (!asset.thumbnailTexture && (asset.type == AssetType::Model || asset.type == AssetType::Material)) {
            asset.thumbnailTexture = ThumbnailGenerator::Instance().Get(asset.path.string());
        }

        // サムネイルテクスチャがあれば ImGui 表示用 ID を取得する。
        if (asset.thumbnailTexture) {
            thumbnailId = ImGuiRenderer::GetTextureID(asset.thumbnailTexture.get());
        }

        // サムネイルがあれば画像表示、無ければアイコン表示にする。
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

        // ファイル名を折り返し表示する。
        ImGui::SetNextItemWidth(cellSize);
        ImGui::TextWrapped("%s", asset.fileName.c_str());

        ImGui::EndGroup();

        // グループ全体をクリック可能にするための invisible button を重ねる。
        ImVec2 itemSize = ImGui::GetItemRectSize();
        ImGui::SetCursorPos(startCursorPos);
        ImGui::InvisibleButton("##Interact", itemSize);

        // 右クリックメニューを表示する。
        if (ImGui::BeginPopupContextItem()) {
            if (ImGui::MenuItem(ICON_FA_CIRCLE_INFO " Inspect Asset")) {
                assetContext.SetActiveAsset(asset.path.string(), asset.type);
                selection.SelectAsset(asset.path.string());
            }
            if (asset.type == AssetType::Texture) {
                ImGui::Separator();
                if (ImGui::MenuItem("Create Sprite")) {
                    assetContext.SetActiveAsset(asset.path.string(), asset.type);
                    CreateSpriteLikeEntityFromTexture(m_registry, asset.path, false);
                }
                if (ImGui::MenuItem("Create UI Button")) {
                    assetContext.SetActiveAsset(asset.path.string(), asset.type);
                    CreateSpriteLikeEntityFromTexture(m_registry, asset.path, true);
                }
                const EntityID selectedEntity = selection.GetPrimaryEntity();
                const bool canAssignToSprite =
                    m_registry &&
                    selection.GetType() == SelectionType::Entity &&
                    !Entity::IsNull(selectedEntity) &&
                    m_registry->GetComponent<SpriteComponent>(selectedEntity) != nullptr;
                if (ImGui::MenuItem("Assign to Selected Sprite", nullptr, false, canAssignToSprite)) {
                    assetContext.SetActiveAsset(asset.path.string(), asset.type);
                    AssignTextureToSelectedSprite(m_registry, asset.path);
                }
            }
            ImGui::Separator();
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

        // ファイル系アセットならドラッグ元になれる。
        if (asset.type != AssetType::Folder && ImGui::BeginDragDropSource()) {
            std::string pathStr = asset.path.string();
            ImGui::SetDragDropPayload("ENGINE_ASSET", pathStr.c_str(), pathStr.size() + 1);
            ImGui::Text("Placing: %s", asset.fileName.c_str());
            ImGui::EndDragDropSource();
        }

        // ダブルクリック時の動作。
        if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
            if (asset.type == AssetType::Folder) {
                // フォルダなら中へ入る。
                m_currentDirectory = asset.path;
            }
            else if (IsSceneAssetPath(asset.path)) {
                // シーンならロード確認ポップアップを開く。
                selection.SelectAsset(asset.path.string());
                m_loadSceneTarget = asset.path.string();
                m_openLoadScenePopup = true;
            }
            else if (asset.type == AssetType::Audio) {
                // 音声ならプレビュー再生を切り替える。
                selection.SelectAsset(asset.path.string());
                EngineKernel::Instance().GetAudioWorld().TogglePreviewClip(asset.path.string(), AudioBusType::UI);
            }
            else {
                // それ以外は外部エディタで開く。
                AssetManager::Instance().OpenInExternalEditor(asset.path);
            }
        }
        else if (ImGui::IsItemClicked(ImGuiMouseButton_Left)) {
            assetContext.SetActiveAsset(asset.path.string(), asset.type);
            if (selection.GetType() != SelectionType::Entity) {
                selection.SelectAsset(asset.path.string());
            }
        }

        // フォルダにはドラッグ&ドロップの受け皿を付ける。
        if (asset.type == AssetType::Folder) {
            if (ImGui::BeginDragDropTarget()) {
                // アセット移動を受け付ける。
                if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ENGINE_ASSET")) {

                    std::string sourcePathStr((const char*)payload->Data);
                    std::filesystem::path sourcePath(sourcePathStr);

                    if (sourcePath.parent_path() != asset.path && sourcePath != asset.path) {
                        AssetManager::Instance().MoveAsset(sourcePath, asset.path);
                        EditorSelection::Instance().Clear();
                    }
                }

                // entity から prefab 保存も受け付ける。
                if (m_registry) {
                    if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ENGINE_ENTITY")) {
                        if (payload->DataSize == sizeof(EntityID)) {
                            const EntityID entity = *static_cast<const EntityID*>(payload->Data);
                            std::filesystem::path prefabPath;
                            if (PrefabSystem::SaveEntityAsPrefab(entity, *m_registry, asset.path, &prefabPath)) {
                                EditorSelection::Instance().SelectAsset(prefabPath.string());
                            }
                        }
                    }
                }
                ImGui::EndDragDropTarget();
            }
        }

        ImGui::PopID();
        ImGui::NextColumn();
    }

    // カラム状態を戻す。
    ImGui::Columns(1);

    // 現在見えているモデル/マテリアルを ThumbnailGenerator へ通知する。
    ThumbnailGenerator::Instance().SetVisiblePaths(visiblePaths);

    // 何もない領域でも entity ドロップを受けられるようにダミー領域を作る。
    ImVec2 dropSurface = ImGui::GetContentRegionAvail();
    if (dropSurface.x <= 0.0f) dropSurface.x = 1.0f;
    if (dropSurface.y < 64.0f) dropSurface.y = 64.0f;
    ImGui::Dummy(dropSurface);

    // 空き領域への entity ドロップで prefab 保存を行う。
    if (ImGui::BeginDragDropTarget()) {
        if (m_registry) {
            if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ENGINE_ENTITY")) {
                if (payload->DataSize == sizeof(EntityID)) {
                    const EntityID entity = *static_cast<const EntityID*>(payload->Data);
                    std::filesystem::path prefabPath;
                    if (PrefabSystem::SaveEntityAsPrefab(entity, *m_registry, m_currentDirectory, &prefabPath)) {
                        EditorSelection::Instance().SelectAsset(prefabPath.string());
                    }
                }
            }
        }
        ImGui::EndDragDropTarget();
    }

    // 空き領域の右クリックメニュー。
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

        // クリップボードに有効なパスがある時だけ Paste を許可する。
        bool canPaste = !m_clipboardPath.empty() && std::filesystem::exists(m_clipboardPath);
        if (ImGui::MenuItem(ICON_FA_PASTE " Paste", "Ctrl+V", false, canPaste)) {
            if (m_isCut) {
                AssetManager::Instance().MoveAsset(m_clipboardPath, m_currentDirectory);
                m_clipboardPath.clear();
            }
            else {
                AssetManager::Instance().CopyAsset(m_clipboardPath, m_currentDirectory);
            }
        }
        ImGui::EndPopup();
    }

    // シーンロード確認ポップアップを開く。
    if (m_openLoadScenePopup) {
        ImGui::OpenPopup("Load Scene");
        m_openLoadScenePopup = false;
    }

    if (ImGui::BeginPopupModal("Load Scene", NULL, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("Load this scene?\n\nCurrent scene changes will be discarded.\n\n%s", m_loadSceneTarget.c_str());
        ImGui::Separator();
        if (ImGui::Button("Yes, Load", ImVec2(120, 0))) {
            m_pendingSceneLoadPath = m_loadSceneTarget;
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(120, 0))) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    // リネームポップアップを開く。
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
        if (ImGui::Button("Cancel", ImVec2(120, 0))) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    // 削除確認ポップアップを開く。
    if (m_openDeletePopup) {
        ImGui::OpenPopup("Delete Asset");
        m_openDeletePopup = false;
    }

    if (ImGui::BeginPopupModal("Delete Asset", NULL, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("Are you sure you want to delete this?\n\n%s", m_deleteTarget.c_str());
        ImGui::Separator();
        if (ImGui::Button("Yes, Delete", ImVec2(120, 0))) {
            AssetManager::Instance().DeleteAsset(m_deleteTarget);
            EditorSelection::Instance().Clear();
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(120, 0))) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
}