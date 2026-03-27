#include "AssetBrowser.h"
#include "AssetManager.h"
#include "PrefabSystem.h"
#include "Registry/Registry.h"
#include "ThumbnailGenerator.h"
#include "Engine/EditorSelection.h"
#include "Graphics.h"
#include "ImGuiRenderer.h"
#include <unordered_set>
#include <cstring>
#include <imgui.h>

namespace
{
    bool IsSceneAssetPath(const std::filesystem::path& path)
    {
        const std::string filename = path.filename().string();
        const char* legacyExtension = ".scene.json";
        const size_t legacyLength = std::strlen(legacyExtension);
        const bool isLegacyScene = filename.size() >= legacyLength &&
            filename.compare(filename.size() - legacyLength, legacyLength, legacyExtension) == 0;
        return path.extension() == ".scene" || isLegacyScene;
    }
}


bool AssetBrowser::ConsumePendingSceneLoad(std::filesystem::path& outPath)
{
    if (m_pendingSceneLoadPath.empty()) {
        return false;
    }

    outPath = m_pendingSceneLoadPath;
    m_pendingSceneLoadPath.clear();
    return true;
}

void AssetBrowser::Initialize() {
    AssetManager::Instance().Initialize("Data");
    m_currentDirectory = AssetManager::Instance().GetRootDirectory();
}

void AssetBrowser::RenderUI() {
    ImGui::Begin(ICON_FA_FOLDER_OPEN " Asset Browser", nullptr, ImGuiWindowFlags_MenuBar);

    if (ImGui::BeginMenuBar()) {
        if (ImGui::BeginMenu("View(V)")) { ImGui::EndMenu(); }
        ImGui::EndMenuBar();
    }

    RenderTopBar();

    ImGui::Separator();

    ImGui::Columns(2, "AssetBrowserSplitter", true);
    if (ImGui::GetColumnWidth() == 0) ImGui::SetColumnWidth(0, 200.0f);

    ImGui::BeginChild("FolderTreeChild", ImVec2(0, 0), false, ImGuiWindowFlags_HorizontalScrollbar);
    std::filesystem::path parentDir = std::filesystem::path(AssetManager::Instance().GetRootDirectory()).parent_path();

    RenderFolderTree(parentDir / "Data");
    RenderFolderTree(parentDir / "Source");
    RenderFolderTree(parentDir / "Shader");

    ImGui::EndChild();

    ImGui::NextColumn();

    ImGui::BeginChild("ContentGridChild");
    RenderContentGrid();
    ImGui::EndChild();

    ImGui::Columns(1);
    ImGui::End();
}


void AssetBrowser::RenderTopBar() {
    std::filesystem::path rootDir = std::filesystem::path(AssetManager::Instance().GetRootDirectory()).parent_path();

    if (IconFontManager::Instance().IconButton(ICON_FA_ARROW_UP)) {
        if (m_currentDirectory != rootDir && m_currentDirectory.has_parent_path()) {
            m_currentDirectory = m_currentDirectory.parent_path();
        }
    }
    ImGui::SameLine();

    // ==========================================
    // ==========================================
    std::vector<std::filesystem::path> pathParts;
    std::filesystem::path tempPath = m_currentDirectory;

    while (tempPath != rootDir && tempPath.has_parent_path()) {
        pathParts.push_back(tempPath);
        tempPath = tempPath.parent_path();
    }

    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(1.0f, 1.0f, 1.0f, 0.2f));

    for (int i = (int)pathParts.size() - 1; i >= 0; --i) {
        std::string folderName = pathParts[i].filename().string();

        if (ImGui::Button(folderName.c_str())) {
            m_currentDirectory = pathParts[i];
        }

        if (i > 0) {
            ImGui::SameLine();
            ImGui::TextDisabled(">");
            ImGui::SameLine();
        }
    }

    ImGui::PopStyleColor(2);

    // ==========================================
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
    // ==========================================
    bool isOpen = ImGui::TreeNodeEx(pathString.c_str(), flags, "");
    bool isNodeClicked = ImGui::IsItemClicked();

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

    ImGui::SameLine();
    ImGui::PushFont(IconFontManager::Instance().GetFontInternal(IconFontSize::Mini));
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.9f, 0.7f, 0.2f, 1.0f));
    ImGui::Text(ICON_FA_FOLDER);
    ImGui::PopStyleColor();
    ImGui::PopFont();

    ImGui::SameLine();
    ImGui::Text("%s", folderName.c_str());

    // ==========================================
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
    std::unordered_set<std::string> visiblePaths;

    ImGui::Columns(columnCount, 0, false);

    auto assets = AssetManager::Instance().GetAssetsInDirectory(m_currentDirectory);


    for (auto& asset : assets) {
        if (!m_searchFilter.empty() && asset.fileName.find(m_searchFilter) == std::string::npos) continue;
        if (asset.type == AssetType::Model || asset.type == AssetType::Material) {
            visiblePaths.insert(asset.path.string());
        }

        ImGui::PushID(asset.path.string().c_str());

        // ==========================================
        // ==========================================
        ImVec2 startCursorPos = ImGui::GetCursorPos();

        ImGui::BeginGroup();

        auto& selection = EditorSelection::Instance();
        bool isSelected = (selection.GetType() == SelectionType::Asset && selection.GetAssetPath() == asset.path.string());
        if (isSelected) {
            ImVec2 p_min = ImGui::GetCursorScreenPos();
            ImVec2 p_max = ImVec2(p_min.x + cellSize, p_min.y + cellSize + ImGui::GetTextLineHeightWithSpacing() * 2);
            ImGui::GetWindowDrawList()->AddRectFilled(p_min, p_max, IM_COL32(50, 100, 200, 128));
        }

        void* thumbnailId = nullptr;
        if (!asset.thumbnailTexture && (asset.type == AssetType::Model || asset.type == AssetType::Material)) {
            asset.thumbnailTexture = ThumbnailGenerator::Instance().Get(asset.path.string());
        }
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


        // ==========================================
        // ==========================================
        ImVec2 itemSize = ImGui::GetItemRectSize();

        ImGui::SetCursorPos(startCursorPos);
        ImGui::InvisibleButton("##Interact", itemSize);


        // ==========================================
        // ==========================================

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

        if (asset.type != AssetType::Folder && ImGui::BeginDragDropSource()) {
            std::string pathStr = asset.path.string();
            ImGui::SetDragDropPayload("ENGINE_ASSET", pathStr.c_str(), pathStr.size() + 1);
            ImGui::Text("Placing: %s", asset.fileName.c_str());
            ImGui::EndDragDropSource();
        }

        if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
            if (asset.type == AssetType::Folder) {
                m_currentDirectory = asset.path;
            }
            else if (IsSceneAssetPath(asset.path)) {
                selection.SelectAsset(asset.path.string());
                m_loadSceneTarget = asset.path.string();
                m_openLoadScenePopup = true;
            }
            else {
                AssetManager::Instance().OpenInExternalEditor(asset.path);
            }
        }
        else if (ImGui::IsItemClicked(ImGuiMouseButton_Left)) {
            selection.SelectAsset(asset.path.string());
        }

        if (asset.type == AssetType::Folder) {
            if (ImGui::BeginDragDropTarget()) {
                if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ENGINE_ASSET")) {

                    std::string sourcePathStr((const char*)payload->Data);
                    std::filesystem::path sourcePath(sourcePathStr);

                    if (sourcePath.parent_path() != asset.path && sourcePath != asset.path) {
                        AssetManager::Instance().MoveAsset(sourcePath, asset.path);
                        EditorSelection::Instance().Clear();
                    }
                }
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


    ImGui::Columns(1);
    ThumbnailGenerator::Instance().SetVisiblePaths(visiblePaths);

    // Keep a tangible drop surface in the content panel so dropping an entity onto
    // empty space reliably creates a prefab in the current folder.
    ImVec2 dropSurface = ImGui::GetContentRegionAvail();
    if (dropSurface.x <= 0.0f) dropSurface.x = 1.0f;
    if (dropSurface.y < 64.0f) dropSurface.y = 64.0f;
    ImGui::Dummy(dropSurface);

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
                m_clipboardPath.clear();
            }
            else {
                AssetManager::Instance().CopyAsset(m_clipboardPath, m_currentDirectory);
            }
        }
        ImGui::EndPopup();
    }

    // ==========================================
    // ==========================================
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
        if (ImGui::Button("Cancel", ImVec2(120, 0))) { ImGui::CloseCurrentPopup(); }
        ImGui::EndPopup();
    }

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
            EditorSelection::Instance().Clear();
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(120, 0))) { ImGui::CloseCurrentPopup(); }
        ImGui::EndPopup();
    }

}

