// Asset picker logic for EffectEditorPanel — extracted from EffectEditorPanel.cpp
// to keep the main panel focused on dock layout / orchestration.
//
// All entry points remain member functions of EffectEditorPanel; only their
// definitions live here.

#include "EffectEditorPanel.h"
#include "EffectEditorPanelInternal.h"

#include <algorithm>
#include <filesystem>
#include <imgui.h>

#include "Asset/ThumbnailGenerator.h"
#include "Icon/IconsFontAwesome7.h"
#include "ImGuiRenderer.h"
#include "System/ResourceManager.h"

using namespace EffectEditorInternal;

void EffectEditorPanel::OpenAssetPicker(AssetPickerKind kind, uint32_t nodeId, bool targetPreviewMesh)
{
    m_assetPickerKind = kind;
    m_assetPickerNodeId = nodeId;
    m_assetPickerTargetsPreviewMesh = targetPreviewMesh;
    m_assetPickerView = AssetPickerView::All;
    m_assetPickerSearch[0] = '\0';
    m_assetPickerOpenRequested = true;
}

bool EffectEditorPanel::DrawAssetSlotControl(const char* label, std::string& path, AssetPickerKind kind, uint32_t nodeId, bool targetPreviewMesh)
{
    bool changed = false;

    ImGui::TextUnformatted(label);
    ImGui::PushID(label);
    ImGui::BeginChild("##AssetSlot", ImVec2(0.0f, 92.0f), true);

    void* textureId = nullptr;
    if (!path.empty()) {
        if (kind == AssetPickerKind::Mesh) {
            ThumbnailGenerator::Instance().Request(path);
            if (auto thumb = ThumbnailGenerator::Instance().Get(path)) {
                textureId = ImGuiRenderer::GetTextureID(thumb.get());
            }
        } else if (kind == AssetPickerKind::Texture) {
            if (auto texture = ResourceManager::Instance().GetTexture(path)) {
                textureId = ImGuiRenderer::GetTextureID(texture.get());
            }
        }
    }

    const ImVec2 previewButtonSize(72.0f, 72.0f);
    bool openPicker = false;
    if (textureId) {
        openPicker = ImGui::ImageButton("##Thumb", textureId, previewButtonSize);
    } else {
        openPicker = ImGui::Button(kind == AssetPickerKind::Mesh ? ICON_FA_CUBE : ICON_FA_IMAGE, previewButtonSize);
    }
    changed |= AcceptAssetDropPayload(path, kind);
    if (openPicker) {
        OpenAssetPicker(kind, nodeId, targetPreviewMesh);
    }

    ImGui::SameLine();
    ImGui::BeginGroup();
    ImGui::TextWrapped("%s", path.empty() ? "No asset assigned" : path.c_str());
    if (!path.empty()) {
        if (ImGui::Button(IsFavoriteAsset(path, kind) ? ICON_FA_STAR " Favorite" : ICON_FA_STAR_HALF_STROKE " Favorite")) {
            ToggleFavoriteAsset(path, kind);
        }
        ImGui::SameLine();
    }
    if (ImGui::Button("Browse")) {
        OpenAssetPicker(kind, nodeId, targetPreviewMesh);
    }
    ImGui::SameLine();
    if (ImGui::Button("Clear")) {
        path.clear();
        changed = true;
    }
    ImGui::EndGroup();
    ImGui::EndChild();
    ImGui::PopID();

    if (changed) {
        m_compileDirty = true;
    }
    return changed;
}

std::string* EffectEditorPanel::GetAssetPickerTargetPath()
{
    if (m_assetPickerTargetsPreviewMesh && m_assetPickerKind == AssetPickerKind::Mesh) {
        return &m_asset.previewDefaults.previewMeshPath;
    }

    if (EffectGraphNode* targetNode = m_asset.FindNode(m_assetPickerNodeId)) {
        return &targetNode->stringValue;
    }

    if (m_assetPickerKind == AssetPickerKind::Mesh) {
        return &m_asset.previewDefaults.previewMeshPath;
    }

    return nullptr;
}

void EffectEditorPanel::AssignPickedAsset(const std::string& path)
{
    if (!IsCompatibleAssetPath(path, m_assetPickerKind)) {
        return;
    }

    if (std::string* targetPath = GetAssetPickerTargetPath()) {
        *targetPath = path;
        m_compileDirty = true;
        TouchRecentAsset(path, m_assetPickerKind);
    }

    if (EffectGraphNode* targetNode = m_asset.FindNode(m_assetPickerNodeId)) {
        m_selectedNodeId = targetNode->id;
    }
}

void EffectEditorPanel::TouchRecentAsset(const std::string& path, AssetPickerKind kind)
{
    if (kind == AssetPickerKind::Mesh) {
        PushRecentPath(m_recentMeshAssets, path);
    } else if (kind == AssetPickerKind::Texture) {
        PushRecentPath(m_recentTextureAssets, path);
    }
}

bool EffectEditorPanel::IsFavoriteAsset(const std::string& path, AssetPickerKind kind) const
{
    if (kind == AssetPickerKind::Mesh) {
        return ContainsExactPath(m_favoriteMeshAssets, path);
    }
    if (kind == AssetPickerKind::Texture) {
        return ContainsExactPath(m_favoriteTextureAssets, path);
    }
    return false;
}

void EffectEditorPanel::ToggleFavoriteAsset(const std::string& path, AssetPickerKind kind)
{
    if (kind == AssetPickerKind::Mesh) {
        TogglePath(m_favoriteMeshAssets, path);
    } else if (kind == AssetPickerKind::Texture) {
        TogglePath(m_favoriteTextureAssets, path);
    }
}

bool EffectEditorPanel::IsCompatibleAssetPath(const std::string& path, AssetPickerKind kind) const
{
    const std::filesystem::path fsPath(path);
    if (kind == AssetPickerKind::Mesh) {
        return HasModelExtension(fsPath);
    }
    if (kind == AssetPickerKind::Texture) {
        return HasTextureExtension(fsPath);
    }
    return false;
}

bool EffectEditorPanel::AcceptAssetDropPayload(std::string& path, AssetPickerKind kind)
{
    bool changed = false;
    if (!ImGui::BeginDragDropTarget()) {
        return false;
    }

    if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ENGINE_ASSET")) {
        std::string droppedPath(static_cast<const char*>(payload->Data));
        if (IsCompatibleAssetPath(droppedPath, kind)) {
            path = droppedPath;
            TouchRecentAsset(droppedPath, kind);
            m_compileDirty = true;
            changed = true;
        }
    }

    ImGui::EndDragDropTarget();
    return changed;
}

void EffectEditorPanel::DrawAssetPickerPopup()
{
    if (m_assetPickerOpenRequested) {
        ImGuiViewport* viewport = ImGui::GetMainViewport();
        ImGui::SetNextWindowSize(ImVec2(760.0f, 520.0f), ImGuiCond_Appearing);
        ImGui::SetNextWindowPos(viewport->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
        ImGui::OpenPopup("EffectEditorAssetPicker");
        m_assetPickerOpenRequested = false;
    }

    if (!ImGui::BeginPopupModal("EffectEditorAssetPicker", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        return;
    }

    const bool wantModels = m_assetPickerKind == AssetPickerKind::Mesh;
    const std::string* targetPath = GetAssetPickerTargetPath();
    const char* popupTitle = wantModels ? "Select Mesh" : "Select Texture";
    ImGui::TextUnformatted(popupTitle);
    ImGui::Separator();
    ImGui::SetNextItemWidth(360.0f);
    ImGui::InputTextWithHint("##PickerSearch", "Search assets", m_assetPickerSearch, sizeof(m_assetPickerSearch));

    std::vector<std::filesystem::path> assets;
    if (wantModels) {
        assets = CollectAssets("Data/Model", true);
    } else {
        assets = CollectAssets("Data/Texture", false);
        auto effectTextures = CollectAssets("Data/Effect", false);
        assets.insert(assets.end(), effectTextures.begin(), effectTextures.end());
    }
    std::sort(assets.begin(), assets.end());
    assets.erase(std::unique(assets.begin(), assets.end()), assets.end());

    if (targetPath && !targetPath->empty()) {
        ImGui::TextDisabled("Current: %s", targetPath->c_str());
    } else {
        ImGui::TextDisabled("Current: none");
    }

    std::vector<std::string> allAssetPaths;
    allAssetPaths.reserve(assets.size());
    for (const auto& path : assets) {
        allAssetPaths.push_back(path.generic_string());
    }

    const std::vector<std::string>& favoriteAssets = wantModels ? m_favoriteMeshAssets : m_favoriteTextureAssets;
    const std::vector<std::string>& recentAssets = wantModels ? m_recentMeshAssets : m_recentTextureAssets;

    const auto drawAssetGrid = [&](const std::vector<std::string>& assetPaths, const char* emptyLabel) {
        ImGui::BeginChild("##PickerGrid", ImVec2(720.0f, 420.0f), true);
        const float cellSize = 124.0f;
        const float width = ImGui::GetContentRegionAvail().x;
        const int columns = (std::max)(1, static_cast<int>(width / cellSize));
        ImGui::Columns(columns, "##PickerColumns", false);

        int visibleCount = 0;
        for (const auto& pathString : assetPaths) {
            const std::filesystem::path path(pathString);
            const std::string filename = path.filename().string();
            if (!ContainsInsensitive(filename, m_assetPickerSearch) && !ContainsInsensitive(pathString, m_assetPickerSearch)) {
                continue;
            }

            ++visibleCount;
            void* thumbId = nullptr;
            if (wantModels) {
                ThumbnailGenerator::Instance().Request(pathString);
                if (auto thumb = ThumbnailGenerator::Instance().Get(pathString)) {
                    thumbId = ImGuiRenderer::GetTextureID(thumb.get());
                }
            } else {
                if (auto texture = ResourceManager::Instance().GetTexture(pathString)) {
                    thumbId = ImGuiRenderer::GetTextureID(texture.get());
                }
            }

            ImGui::PushID(pathString.c_str());
            const bool isAssigned = targetPath && *targetPath == pathString;
            const bool isFavorite = IsFavoriteAsset(pathString, m_assetPickerKind);
            if (isAssigned) {
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.20f, 0.40f, 0.70f, 0.90f));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.26f, 0.48f, 0.82f, 0.95f));
                ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.16f, 0.34f, 0.62f, 1.0f));
            }

            bool picked = false;
            if (thumbId) {
                picked = ImGui::ImageButton("##Thumb", thumbId, ImVec2(72.0f, 72.0f));
            } else {
                picked = ImGui::Button(wantModels ? ICON_FA_CUBE : ICON_FA_IMAGE, ImVec2(72.0f, 72.0f));
            }
            if (isAssigned) {
                ImGui::PopStyleColor(3);
            }
            if (picked) {
                AssignPickedAsset(pathString);
                ImGui::CloseCurrentPopup();
            }

            ImGui::SameLine();
            if (ImGui::SmallButton(isFavorite ? ICON_FA_STAR : ICON_FA_STAR_HALF_STROKE)) {
                ToggleFavoriteAsset(pathString, m_assetPickerKind);
            }
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip(isFavorite ? "Remove from favorites" : "Add to favorites");
            }

            if (isAssigned) {
                ImGui::TextColored(ImVec4(0.45f, 0.76f, 1.0f, 1.0f), "%s", filename.c_str());
                ImGui::TextDisabled("Assigned");
            } else {
                ImGui::TextWrapped("%s", filename.c_str());
            }
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("%s", pathString.c_str());
            }
            ImGui::NextColumn();
            ImGui::PopID();
        }

        ImGui::Columns(1);
        if (visibleCount == 0) {
            ImGui::TextDisabled("%s", emptyLabel);
        }
        ImGui::EndChild();
    };

    if (ImGui::BeginTabBar("##EffectAssetPickerTabs")) {
        if (ImGui::BeginTabItem("All")) {
            drawAssetGrid(allAssetPaths, "No matching assets.");
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Favorites")) {
            drawAssetGrid(favoriteAssets, "No favorite assets yet.");
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Recent")) {
            drawAssetGrid(recentAssets, "No recent assets yet.");
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }

    if (ImGui::Button("Close")) {
        ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
}
