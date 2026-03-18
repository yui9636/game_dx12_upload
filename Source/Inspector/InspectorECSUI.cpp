#include "InspectorECSUI.h"
#include "Engine/EditorSelection.h"
#include "System/ResourceManager.h"
#include "Material/MaterialPreviewStudio.h"
#include "ImGuiRenderer.h"
#include "Graphics.h"
#include "Icon/IconsFontAwesome7.h"
#include <imgui.h>
#include <filesystem>
#include <algorithm>

namespace {
    void DrawTextureInspector(const std::string& path, const std::string& filename) {
        ImGui::Text("Texture");
        ImGui::TextWrapped("%s", filename.c_str());
        ImGui::Separator();

        auto texture = ResourceManager::Instance().GetTexture(path);
        if (!texture) {
            ImGui::TextColored(ImVec4(1, 0, 0, 1), "Failed to load texture.");
            return;
        }

        const int width = static_cast<int>(texture->GetWidth());
        const int height = static_cast<int>(texture->GetHeight());
        ImGui::Text("Resolution: %d x %d", width, height);

        if (width > 0 && height > 0) {
            float displayWidth = ImGui::GetContentRegionAvail().x;
            if (displayWidth > 250.0f) displayWidth = 250.0f;
            const float aspect = static_cast<float>(height) / static_cast<float>(width);
            if (void* textureId = ImGuiRenderer::GetTextureID(texture.get())) {
                ImGui::Image((ImTextureID)textureId, ImVec2(displayWidth, displayWidth * aspect));
            } else {
                ImGui::TextDisabled("Preview unavailable on this renderer.");
            }
        }
    }

    void DrawModelInspector(const std::string& path, const std::string& filename) {
        ImGui::Text("Model");
        ImGui::TextWrapped("%s", filename.c_str());
        ImGui::Separator();

        auto model = ResourceManager::Instance().GetModel(path);
        if (!model) {
            ImGui::TextColored(ImVec4(1, 0, 0, 1), "Failed to load model.");
            return;
        }

        ImGui::Text("Meshes: %d", static_cast<int>(model->GetMeshes().size()));
        ImGui::Text("Materials: %d", static_cast<int>(model->GetMaterials().size()));
        ImGui::Text("Nodes: %d", static_cast<int>(model->GetNodes().size()));
        ImGui::Text("Animations: %d", static_cast<int>(model->GetAnimations().size()));
    }

    void DrawMaterialInspector(const std::string& path, const std::string& filename) {
        ImGui::Text("Material");
        ImGui::TextWrapped("%s", filename.c_str());
        ImGui::Separator();

        auto material = ResourceManager::Instance().GetMaterial(path);
        if (!material) {
            ImGui::TextColored(ImVec4(1, 0, 0, 1), "Failed to load material.");
            return;
        }

        ImGui::ColorEdit4("Base Color", &material->baseColor.x);
        ImGui::SliderFloat("Metallic", &material->metallic, 0.0f, 1.0f);
        ImGui::SliderFloat("Roughness", &material->roughness, 0.0f, 1.0f);
        ImGui::SliderFloat("Emissive", &material->emissive, 0.0f, 10.0f);

        if (Graphics::Instance().GetAPI() == GraphicsAPI::DX11) {
            auto previewSRV = MaterialPreviewStudio::Instance().GetPreviewSRV();
            if (previewSRV) {
                const float width = ImGui::GetContentRegionAvail().x;
                const float aspect = Graphics::Instance().GetScreenHeight() / Graphics::Instance().GetScreenWidth();
                ImGui::Image((ImTextureID)previewSRV, ImVec2(width, width * aspect));
            }
        } else {
            ImGui::TextDisabled("DX12 preview is not available yet.");
        }
    }

    void DrawAssetInspector(const std::string& assetPath) {
        std::filesystem::path path(assetPath);
        const std::string filename = path.filename().string();
        std::string ext = path.extension().string();
        std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

        if (ext == ".png" || ext == ".jpg" || ext == ".tga" || ext == ".dds" || ext == ".hdr") {
            DrawTextureInspector(assetPath, filename);
            return;
        }
        if (ext == ".fbx" || ext == ".obj" || ext == ".blend" || ext == ".gltf" || ext == ".cereal") {
            DrawModelInspector(assetPath, filename);
            return;
        }
        if (ext == ".mat") {
            DrawMaterialInspector(assetPath, filename);
            return;
        }

        ImGui::TextWrapped("%s", filename.c_str());
        ImGui::Separator();
        ImGui::TextWrapped("Path: %s", assetPath.c_str());
    }
}

void InspectorECSUI::Render(Registry* registry) {
    (void)registry;
    ImGui::Begin(ICON_FA_CIRCLE_INFO " Inspector");

    auto& selection = EditorSelection::Instance();
    switch (selection.GetType()) {
    case SelectionType::Asset:
        DrawAssetInspector(selection.GetAssetPath());
        break;
    case SelectionType::Entity:
        ImGui::Text("Entity");
        ImGui::Separator();
        ImGui::Text("Entity ID: %llu", static_cast<unsigned long long>(selection.GetEntity()));
        ImGui::TextDisabled("Detailed ECS inspector is temporarily simplified.");
        break;
    default:
        ImGui::TextDisabled("Nothing selected.");
        break;
    }

    ImGui::End();
}
