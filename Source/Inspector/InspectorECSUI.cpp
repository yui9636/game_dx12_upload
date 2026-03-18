#include "InspectorECSUI.h"
#include "Engine/EditorSelection.h"
#include "System/ResourceManager.h"
#include "Material/MaterialPreviewStudio.h"
#include "ImGuiRenderer.h"
#include "Graphics.h"
#include "Generated/ComponentMeta.generated.h"
#include "Icon/IconsFontAwesome7.h"
#include "Component/NameComponent.h"
#include "Component/TransformComponent.h"
#include "Component/MeshComponent.h"
#include "Component/LightComponent.h"
#include "Component/HierarchyComponent.h"
#include "Component/MaterialComponent.h"
#include "Component/CameraComponent.h"
#include "Component/EnvironmentComponent.h"
#include "Component/PostEffectComponent.h"
#include "Component/ReflectionProbeComponent.h"
#include "Component/ShadowSettingsComponent.h"
#include "Registry/Registry.h"
#include <imgui.h>
#include <filesystem>
#include <algorithm>
#include <memory>
#include <type_traits>

namespace {
    template <typename T>
    struct IsSharedPtr : std::false_type {};

    template <typename T>
    struct IsSharedPtr<std::shared_ptr<T>> : std::true_type {};

    template <typename T>
    bool DrawValueWidget(const char* label, T& value) {
        ImGui::PushID(label);
        ImGui::AlignTextToFramePadding();
        ImGui::TextDisabled("%s", label);
        ImGui::TableNextColumn();
        ImGui::SetNextItemWidth(-1.0f);

        bool changed = false;
        if constexpr (std::is_same_v<T, bool>) {
            changed = ImGui::Checkbox("##value", &value);
        } else if constexpr (std::is_same_v<T, float>) {
            changed = ImGui::DragFloat("##value", &value, 0.01f);
        } else if constexpr (std::is_same_v<T, int>) {
            changed = ImGui::DragInt("##value", &value, 1.0f);
        } else if constexpr (std::is_same_v<T, uint32_t>) {
            int temp = static_cast<int>(value);
            if (ImGui::DragInt("##value", &temp, 1.0f, 0)) {
                value = static_cast<uint32_t>(temp);
                changed = true;
            }
        } else if constexpr (std::is_same_v<T, std::string>) {
            char buffer[512] = {};
            strcpy_s(buffer, value.c_str());
            if (ImGui::InputText("##value", buffer, sizeof(buffer))) {
                value = buffer;
                changed = true;
            }
        } else if constexpr (std::is_same_v<T, DirectX::XMFLOAT3>) {
            changed = ImGui::DragFloat3("##value", &value.x, 0.01f);
        } else if constexpr (std::is_same_v<T, DirectX::XMFLOAT4>) {
            changed = ImGui::DragFloat4("##value", &value.x, 0.01f);
        } else if constexpr (std::is_same_v<T, EntityID>) {
            ImGui::Text("%llu", static_cast<unsigned long long>(value));
        } else if constexpr (std::is_enum_v<T>) {
            int enumValue = static_cast<int>(value);
            if (ImGui::DragInt("##value", &enumValue, 1.0f, 0)) {
                value = static_cast<T>(enumValue);
                changed = true;
            }
        } else if constexpr (std::is_same_v<T, DirectX::XMFLOAT4X4>) {
            ImGui::TextDisabled("Matrix");
        } else if constexpr (IsSharedPtr<T>::value) {
            ImGui::TextDisabled(value ? "Loaded" : "None");
        } else {
            ImGui::TextDisabled("Unsupported");
        }

        ImGui::PopID();
        ImGui::TableNextColumn();
        return changed;
    }

    template <typename T>
    void DrawComponentBlock(const char* label, T& component) {
        if (!ImGui::CollapsingHeader(label, ImGuiTreeNodeFlags_DefaultOpen)) {
            return;
        }

        if (ImGui::BeginTable(label, 2, ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_BordersInnerV)) {
            ImGui::TableSetupColumn("Field", ImGuiTableColumnFlags_WidthFixed, 180.0f);
            ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableNextColumn();

            std::apply([&](auto... fields) {
                ((DrawValueWidget(fields.name.data(), component.*(fields.ptr))), ...);
            }, ComponentMeta<T>::Fields);

            ImGui::EndTable();
        }
    }

    template <typename T>
    void DrawComponentIfPresent(Registry* registry, EntityID entity) {
        if (T* component = registry->GetComponent<T>(entity)) {
            DrawComponentBlock(ComponentMeta<T>::Name.data(), *component);
        }
    }

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
    ImGui::Begin(ICON_FA_CIRCLE_INFO " Inspector");

    auto& selection = EditorSelection::Instance();
    switch (selection.GetType()) {
    case SelectionType::Asset:
        DrawAssetInspector(selection.GetAssetPath());
        break;
    case SelectionType::Entity:
        if (!registry) {
            ImGui::TextDisabled("Registry unavailable.");
            break;
        }
        {
            const EntityID entity = selection.GetEntity();
            ImGui::Text("Entity");
            ImGui::Separator();
            ImGui::Text("Entity ID: %llu", static_cast<unsigned long long>(entity));

            if (auto* name = registry->GetComponent<NameComponent>(entity)) {
                ImGui::Text("Name: %s", name->name.c_str());
            }

            ImGui::Spacing();
            DrawComponentIfPresent<NameComponent>(registry, entity);
            DrawComponentIfPresent<TransformComponent>(registry, entity);
            DrawComponentIfPresent<MeshComponent>(registry, entity);
            DrawComponentIfPresent<MaterialComponent>(registry, entity);
            DrawComponentIfPresent<HierarchyComponent>(registry, entity);
            DrawComponentIfPresent<LightComponent>(registry, entity);
            DrawComponentIfPresent<CameraFreeControlComponent>(registry, entity);
            DrawComponentIfPresent<CameraLensComponent>(registry, entity);
            DrawComponentIfPresent<EnvironmentComponent>(registry, entity);
            DrawComponentIfPresent<PostEffectComponent>(registry, entity);
            DrawComponentIfPresent<ReflectionProbeComponent>(registry, entity);
            DrawComponentIfPresent<ShadowSettingsComponent>(registry, entity);
        }
        break;
    default:
        ImGui::TextDisabled("Nothing selected.");
        break;
    }

    ImGui::End();
}
