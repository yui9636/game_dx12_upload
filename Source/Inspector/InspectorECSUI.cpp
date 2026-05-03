//#include "InspectorECSUI.h"
//
//#include "Engine/EditorSelection.h"
//
//#include "Engine/EngineKernel.h"
//
//#include "System/ResourceManager.h"
//
//#include "Material/MaterialPreviewStudio.h"
//
//#include "Material/MaterialAsset.h"
//
//#include "Asset/ThumbnailGenerator.h"
//
//#include "Asset/PrefabSystem.h"
//
//#include "Asset/AssetManager.h"
//
//#include "ImGuiRenderer.h"
//
//#include "Graphics.h"
//
//#include "Generated/ComponentMeta.generated.h"
//
//#include "Icon/IconsFontAwesome7.h"
//
//#include "Component/NameComponent.h"
//
//// Gameplay components
//#include "Gameplay/PlayerTagComponent.h"
//#include "Gameplay/CharacterPhysicsComponent.h"
//#include "Gameplay/HealthComponent.h"
//#include "Gameplay/StaminaComponent.h"
//#include "Gameplay/LocomotionStateComponent.h"
//#include "Gameplay/ActionStateComponent.h"
//#include "Gameplay/ActionDatabaseComponent.h"
//#include "Gameplay/DodgeStateComponent.h"
//#include "Gameplay/HitboxTrackingComponent.h"
//#include "Gameplay/StageBoundsComponent.h"
//#include "Gameplay/PlaybackComponent.h"
//#include "Gameplay/PlaybackRangeComponent.h"
//#include "Gameplay/TimelineComponent.h"
//#include "Gameplay/SpeedCurveComponent.h"
//#include "Gameplay/HitStopComponent.h"
//#include "Component/EffectAssetComponent.h"
//#include "Component/EffectAttachmentComponent.h"
//#include "Component/EffectParameterOverrideComponent.h"
//#include "Component/EffectPlaybackComponent.h"
//#include "Component/EffectPreviewTagComponent.h"
//#include "Component/EffectSpawnRequestComponent.h"
//#include "Component/SequencerPreviewCameraComponent.h"
//// Input components
//#include "Input/InputUserComponent.h"
//#include "Input/InputContextComponent.h"
//#include "Input/InputBindingComponent.h"
//#include "Input/ResolvedInputStateComponent.h"
//#include "Input/InputTextFieldComponent.h"
//#include "Input/VibrationRequestComponent.h"
//#include "Component/ColliderComponent.h"
//
//#include "Component/TransformComponent.h"
//
//#include "Component/MeshComponent.h"
//
//#include "Component/LightComponent.h"
//
//#include "Component/HierarchyComponent.h"
//
//#include "Component/MaterialComponent.h"
//
//#include "Component/PrefabInstanceComponent.h"
//#include "Component/NodeAttachmentComponent.h"
//#include "Component/NodeSocketComponent.h"
//
//#include "Component/CameraComponent.h"
//
//#include "Component/AudioEmitterComponent.h"
//#include "Component/AudioBusSendComponent.h"
//#include "Component/AudioListenerComponent.h"
//
//#include "Component/AudioSettingsComponent.h"
//
//#include "Component/Camera2DComponent.h"
//
//#include "Component/CanvasItemComponent.h"
//
//#include "Component/EnvironmentComponent.h"
//
//#include "Component/PostEffectComponent.h"
//
//#include "Component/RectTransformComponent.h"
//
//#include "Component/ReflectionProbeComponent.h"
//
//#include "Component/ShadowSettingsComponent.h"
//
//#include "Component/SpriteComponent.h"
//
//#include "Component/TextComponent.h"
//
//#include "Registry/Registry.h"
//
//#include "System/UndoSystem.h"
//
//#include "Undo/ComponentUndoAction.h"
//
//#include "Undo/EntitySnapshot.h"
//
//#include "Undo/EntityUndoActions.h"
//
//#include <imgui.h>
//
//#include <filesystem>
//
//#include <algorithm>
//
//#include <fstream>
//
//#include <memory>
//
//#include <type_traits>
//
//#include <unordered_map>
//
//
//
//namespace {
//
//    template <typename T>
//
//    struct IsSharedPtr : std::false_type {};
//
//
//
//    template <typename T>
//
//    struct IsSharedPtr<std::shared_ptr<T>> : std::true_type {};
//
//
//
//    template <typename T>
//
//    bool DrawValueWidget(const char* label, T& value) {
//
//        ImGui::PushID(label);
//
//        ImGui::AlignTextToFramePadding();
//
//        ImGui::TextDisabled("%s", label);
//
//        ImGui::TableNextColumn();
//
//        ImGui::SetNextItemWidth(-1.0f);
//
//
//
//        bool changed = false;
//
//        if constexpr (std::is_same_v<T, bool>) {
//
//            changed = ImGui::Checkbox("##value", &value);
//
//        } else if constexpr (std::is_same_v<T, float>) {
//
//            changed = ImGui::DragFloat("##value", &value, 0.01f);
//
//        } else if constexpr (std::is_same_v<T, int>) {
//
//            changed = ImGui::DragInt("##value", &value, 1.0f);
//
//        } else if constexpr (std::is_same_v<T, uint32_t>) {
//
//            int temp = static_cast<int>(value);
//
//            if (ImGui::DragInt("##value", &temp, 1.0f, 0)) {
//
//                value = static_cast<uint32_t>(temp);
//
//                changed = true;
//
//            }
//
//        } else if constexpr (std::is_same_v<T, std::string>) {
//
//            char buffer[512] = {};
//
//            strcpy_s(buffer, value.c_str());
//
//            if (ImGui::InputText("##value", buffer, sizeof(buffer))) {
//
//                value = buffer;
//
//                changed = true;
//
//            }
//
//        } else if constexpr (std::is_same_v<T, DirectX::XMFLOAT2>) {
//
//            changed = ImGui::DragFloat2("##value", &value.x, 0.01f);
//
//        } else if constexpr (std::is_same_v<T, DirectX::XMFLOAT3>) {
//
//            changed = ImGui::DragFloat3("##value", &value.x, 0.01f);
//
//        } else if constexpr (std::is_same_v<T, DirectX::XMFLOAT4>) {
//
//            changed = ImGui::DragFloat4("##value", &value.x, 0.01f);
//
//        } else if constexpr (std::is_same_v<T, EntityID>) {
//
//            ImGui::Text("%llu", static_cast<unsigned long long>(value));
//
//        } else if constexpr (std::is_enum_v<T>) {
//
//            int enumValue = static_cast<int>(value);
//
//            if (ImGui::DragInt("##value", &enumValue, 1.0f, 0)) {
//
//                value = static_cast<T>(enumValue);
//
//                changed = true;
//
//            }
//
//        } else if constexpr (std::is_same_v<T, DirectX::XMFLOAT4X4>) {
//
//            ImGui::TextDisabled("Matrix");
//
//        } else if constexpr (IsSharedPtr<T>::value) {
//
//            ImGui::TextDisabled(value ? "Loaded" : "None");
//
//        } else {
//
//            ImGui::TextDisabled("Unsupported");
//
//        }
//
//
//
//        ImGui::PopID();
//
//        ImGui::TableNextColumn();
//
//        return changed;
//
//    }
//
//
//
//    template <typename T>
//
//    struct EditSession {
//
//        T before{};
//
//        bool dirty = false;
//
//    };
//
//
//
//    template <typename T>
//
//    std::unordered_map<uint64_t, EditSession<T>>& GetEditSessions() {
//
//        static std::unordered_map<uint64_t, EditSession<T>> sessions;
//
//        return sessions;
//
//    }
//
//
//
//    template <typename T>
//
//    uint64_t MakeEditSessionKey(EntityID entity) {
//
//        return (static_cast<uint64_t>(Entity::GetIndex(entity)) << 32) | Entity::GetGeneration(entity);
//
//    }
//
//
//
//    template <typename TComponent, typename TValue>
//
//    bool DrawUndoableValueWidget(Registry* registry, EntityID entity, TComponent& component, const char* label, TValue& value) {
//
//        const TComponent beforeWidget = component;
//
//        const bool changed = DrawValueWidget(label, value);
//
//
//
//        auto& sessions = GetEditSessions<TComponent>();
//
//        const uint64_t key = MakeEditSessionKey<TComponent>(entity);
//
//        auto it = sessions.find(key);
//
//
//
//        if ((ImGui::IsItemActivated() || changed) && it == sessions.end()) {
//
//            it = sessions.emplace(key, EditSession<TComponent>{ beforeWidget, false }).first;
//
//        }
//
//
//
//        if (changed && it != sessions.end()) {
//
//            it->second.dirty = true;
//
//        }
//
//
//
//        if (it != sessions.end() && ImGui::IsItemDeactivatedAfterEdit()) {
//
//            if (it->second.dirty) {
//
//                UndoSystem::Instance().ExecuteAction(
//
//                    std::make_unique<ComponentUndoAction<TComponent>>(entity, it->second.before, component),
//
//                    *registry);
//
//                PrefabSystem::MarkPrefabOverride(entity, *registry);
//
//            }
//
//            sessions.erase(it);
//
//        }
//
//
//
//        return changed;
//
//    }
//
//
//
//    template <typename T>
//
//    void DrawComponentBlock(Registry* registry, EntityID entity, const char* label, T& component) {
//
//        if (!ImGui::CollapsingHeader(label, ImGuiTreeNodeFlags_DefaultOpen)) {
//
//            return;
//
//        }
//
//
//
//        if (ImGui::BeginTable(label, 2, ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_BordersInnerV)) {
//
//            ImGui::TableSetupColumn("Field", ImGuiTableColumnFlags_WidthFixed, 180.0f);
//
//            ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);
//
//            ImGui::TableNextColumn();
//
//
//
//            std::apply([&](auto... fields) {
//
//                ((DrawUndoableValueWidget(registry, entity, component, fields.name.data(), component.*(fields.ptr))), ...);
//
//            }, ComponentMeta<T>::Fields);
//
//
//
//            ImGui::EndTable();
//
//        }
//
//    }
//
//
//
//    template <typename T>
//
//    void DrawComponentIfPresent(Registry* registry, EntityID entity) {
//
//        if (T* component = registry->GetComponent<T>(entity)) {
//
//            DrawComponentBlock(registry, entity, ComponentMeta<T>::Name.data(), *component);
//
//        }
//
//    }
//
//    // Removable version - shows X button in header
//    template <typename T>
//    bool DrawComponentRemovable(Registry* registry, EntityID entity) {
//        T* component = registry->GetComponent<T>(entity);
//        if (!component) return false;
//
//        const char* name = ComponentMeta<T>::Name.data();
//        bool open = true;
//        bool hdrOpen = ImGui::CollapsingHeader(name, &open, ImGuiTreeNodeFlags_DefaultOpen);
//
//        if (!open) {
//            // X button clicked — remove component
//            registry->RemoveComponent<T>(entity);
//            return true; // component removed
//        }
//
//        if (hdrOpen) {
//            if (ImGui::BeginTable(name, 2, ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_BordersInnerV)) {
//                ImGui::TableSetupColumn("Field", ImGuiTableColumnFlags_WidthFixed, 180.0f);
//                ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);
//                ImGui::TableNextColumn();
//                std::apply([&](auto... fields) {
//                    ((DrawUndoableValueWidget(registry, entity, *component, fields.name.data(), component->*(fields.ptr))), ...);
//                }, ComponentMeta<T>::Fields);
//                ImGui::EndTable();
//            }
//        }
//        return false;
//    }
//
//    // Add Component helper - returns true if added
//    template <typename T>
//    bool TryAddComponent(Registry* registry, EntityID entity, const char* label) {
//        if (registry->GetComponent<T>(entity)) return false; // already present
//        if (ImGui::MenuItem(label)) {
//            registry->AddComponent(entity, T{});
//            return true;
//        }
//        return false;
//    }
//
//
//
//    void DrawTextureInspector(const std::string& path, const std::string& filename) {
//
//        ImGui::Text("Texture");
//
//        ImGui::TextWrapped("%s", filename.c_str());
//
//        ImGui::Separator();
//
//
//
//        auto texture = ResourceManager::Instance().GetTexture(path);
//
//        if (!texture) {
//
//            ImGui::TextColored(ImVec4(1, 0, 0, 1), "Failed to load texture.");
//
//            return;
//
//        }
//
//
//
//        const int width = static_cast<int>(texture->GetWidth());
//
//        const int height = static_cast<int>(texture->GetHeight());
//
//        ImGui::Text("Resolution: %d x %d", width, height);
//
//
//
//        if (width > 0 && height > 0) {
//
//            float displayWidth = ImGui::GetContentRegionAvail().x;
//
//            if (displayWidth > 250.0f) displayWidth = 250.0f;
//
//            const float aspect = static_cast<float>(height) / static_cast<float>(width);
//
//            if (void* textureId = ImGuiRenderer::GetTextureID(texture.get())) {
//
//                ImGui::Image((ImTextureID)textureId, ImVec2(displayWidth, displayWidth * aspect));
//
//            } else {
//
//                ImGui::TextDisabled("Preview unavailable on this renderer.");
//
//            }
//
//        }
//
//    }
//
//
//
//    void DrawModelInspector(const std::string& path, const std::string& filename) {
//
//        ImGui::Text("Model");
//
//        ImGui::TextWrapped("%s", filename.c_str());
//
//        ImGui::Separator();
//
//
//
//        auto model = ResourceManager::Instance().GetModel(path);
//
//        if (!model) {
//
//            ImGui::TextColored(ImVec4(1, 0, 0, 1), "Failed to load model.");
//
//            return;
//
//        }
//
//
//
//        ImGui::Text("Meshes: %d", static_cast<int>(model->GetMeshes().size()));
//
//        ImGui::Text("Materials: %d", static_cast<int>(model->GetMaterials().size()));
//
//        ImGui::Text("Nodes: %d", static_cast<int>(model->GetNodes().size()));
//
//        ImGui::Text("Animations: %d", static_cast<int>(model->GetAnimations().size()));
//
//    }
//
//
//
//    // Cached texture path list (filenames only, no GPU load)
//
//    struct TexturePathCache {
//
//        std::vector<std::string> paths;
//
//        std::vector<std::string> filenames;
//
//        bool initialized = false;
//
//
//
//        void EnsureBuilt() {
//
//            if (initialized) return;
//
//            initialized = true;
//
//            paths.clear();
//
//            filenames.clear();
//
//            CollectRecursive(AssetManager::Instance().GetRootDirectory());
//
//        }
//
//        void Invalidate() { initialized = false; }
//
//    private:
//
//        void CollectRecursive(const std::filesystem::path& dir) {
//
//            auto entries = AssetManager::Instance().GetAssetsInDirectory(dir);
//
//            for (auto& e : entries) {
//
//                if (e.type == AssetType::Texture) {
//
//                    paths.push_back(e.path.string());
//
//                    filenames.push_back(e.path.filename().string());
//
//                } else if (e.type == AssetType::Folder) {
//
//                    CollectRecursive(e.path);
//
//                }
//
//            }
//
//        }
//
//    };
//
//    static TexturePathCache s_texCache;
//
//
//
//    bool DrawTextureSlot(const char* label, std::string& texPath) {
//
//        ImGui::PushID(label);
//
//        bool changed = false;
//
//
//
//        ImGui::Text("%s", label);
//
//        ImGui::SameLine();
//
//
//
//        const float thumbSize = 48.0f;
//
//
//
//        // Thumbnail (show only the currently assigned texture, not all textures)
//
//        ImVec2 pos = ImGui::GetCursorScreenPos();
//
//        ImGui::InvisibleButton("##slot", ImVec2(thumbSize, thumbSize));
//
//        bool clicked = ImGui::IsItemClicked(0);
//
//        ImVec2 p0 = ImGui::GetItemRectMin();
//
//        ImVec2 p1 = ImGui::GetItemRectMax();
//
//
//
//        ImDrawList* drawList = ImGui::GetWindowDrawList();
//
//        if (!texPath.empty()) {
//
//            auto tex = ResourceManager::Instance().GetTexture(texPath);
//
//            void* texId = tex ? ImGuiRenderer::GetTextureID(tex.get()) : nullptr;
//
//            if (texId) {
//
//                drawList->AddImage((ImTextureID)texId, p0, p1);
//
//            } else {
//
//                drawList->AddRectFilled(p0, p1, IM_COL32(60, 60, 60, 255));
//
//                drawList->AddText(ImVec2(p0.x + 4, p0.y + 16), IM_COL32(255, 255, 0, 255), "?");
//
//            }
//
//        } else {
//
//            drawList->AddRectFilled(p0, p1, IM_COL32(50, 50, 50, 255));
//
//            drawList->AddRect(p0, p1, IM_COL32(100, 100, 100, 255));
//
//        }
//
//        drawList->AddRect(p0, p1, IM_COL32(120, 120, 120, 255));
//
//
//
//        // D&D target
//
//        if (ImGui::BeginDragDropTarget()) {
//
//            if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ENGINE_ASSET")) {
//
//                std::string dropped((const char*)payload->Data);
//
//                std::string ext = std::filesystem::path(dropped).extension().string();
//
//                std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
//
//                if (ext == ".png" || ext == ".jpg" || ext == ".tga" || ext == ".dds" || ext == ".hdr") {
//
//                    texPath = dropped;
//
//                    changed = true;
//
//                }
//
//            }
//
//            ImGui::EndDragDropTarget();
//
//        }
//
//
//
//        // Click to open texture picker popup
//
//        if (clicked) {
//
//            s_texCache.EnsureBuilt();
//
//            ImGui::OpenPopup("##texPicker");
//
//        }
//
//
//
//        if (ImGui::BeginPopup("##texPicker")) {
//
//            static char filterBuf[128] = "";
//
//            ImGui::InputText("Filter", filterBuf, sizeof(filterBuf));
//
//
//
//            ImGui::BeginChild("##texList", ImVec2(300, 200), true);
//
//            for (size_t i = 0; i < s_texCache.paths.size(); ++i) {
//
//                auto& fn = s_texCache.filenames[i];
//
//                if (filterBuf[0] != '\0') {
//
//                    std::string lower = fn;
//
//                    std::string filterLower = filterBuf;
//
//                    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
//
//                    std::transform(filterLower.begin(), filterLower.end(), filterLower.begin(), ::tolower);
//
//                    if (lower.find(filterLower) == std::string::npos) continue;
//
//                }
//
//                bool selected = (s_texCache.paths[i] == texPath);
//
//                auto previewTexture = ResourceManager::Instance().GetTexture(s_texCache.paths[i]);
//
//                void* previewId = previewTexture ? ImGuiRenderer::GetTextureID(previewTexture.get()) : nullptr;
//
//
//
//                ImGui::PushID(static_cast<int>(i));
//
//                if (previewId) {
//
//                    ImGui::Image((ImTextureID)previewId, ImVec2(36.0f, 36.0f));
//
//                } else {
//
//                    ImGui::Dummy(ImVec2(36.0f, 36.0f));
//
//                }
//
//                ImGui::SameLine();
//
//                if (ImGui::Selectable(fn.c_str(), selected, 0, ImVec2(0.0f, 36.0f))) {
//
//                    texPath = s_texCache.paths[i];
//
//                    changed = true;
//
//                    filterBuf[0] = '\0';
//
//                    ImGui::CloseCurrentPopup();
//
//                    ImGui::PopID();
//
//                    break;
//
//                }
//
//                ImGui::PopID();
//
//            }
//
//            ImGui::EndChild();
//
//
//
//            if (ImGui::Button("None")) {
//
//                texPath.clear();
//
//                changed = true;
//
//                filterBuf[0] = '\0';
//
//                ImGui::CloseCurrentPopup();
//
//            }
//
//            ImGui::EndPopup();
//
//        }
//
//
//
//        ImGui::SameLine();
//
//        if (!texPath.empty()) {
//
//            std::string filename = std::filesystem::path(texPath).filename().string();
//
//            ImGui::TextWrapped("%s", filename.c_str());
//
//        } else {
//
//            ImGui::TextDisabled("(empty)");
//
//        }
//
//
//
//        if (!texPath.empty()) {
//
//            ImGui::SameLine();
//
//            if (ImGui::SmallButton("X")) {
//
//                texPath.clear();
//
//                changed = true;
//
//            }
//
//        }
//
//
//
//        ImGui::PopID();
//
//        return changed;
//
//    }
//
//
//
//    void DrawMaterialEditor(MaterialAsset* material) {
//
//        if (!material) return;
//
//
//
//        bool changed = false;
//
//
//
//        const char* shaderNames[] = { "Phong", "PBR", "Toon" };
//
//        int shaderIdx = material->shaderId;
//
//        if (shaderIdx < 0 || shaderIdx > 2) shaderIdx = 1;
//
//        if (ImGui::Combo("Shader", &shaderIdx, shaderNames, IM_ARRAYSIZE(shaderNames))) {
//
//            material->shaderId = shaderIdx;
//
//            changed = true;
//
//        }
//
//
//
//        changed |= ImGui::ColorEdit4("Base Color", &material->baseColor.x);
//
//        changed |= ImGui::SliderFloat("Metallic", &material->metallic, 0.0f, 1.0f);
//
//        changed |= ImGui::SliderFloat("Roughness", &material->roughness, 0.0f, 1.0f);
//
//        changed |= ImGui::SliderFloat("Emissive", &material->emissive, 0.0f, 10.0f);
//
//
//
//        ImGui::Spacing();
//
//        ImGui::Separator();
//
//        ImGui::Text("Textures");
//
//
//
//        changed |= DrawTextureSlot("Diffuse", material->diffuseTexturePath);
//
//        changed |= DrawTextureSlot("Normal", material->normalTexturePath);
//
//        changed |= DrawTextureSlot("MetallicRoughness", material->metallicRoughnessTexturePath);
//
//        changed |= DrawTextureSlot("Emissive", material->emissiveTexturePath);
//
//        ImGui::Separator();
//
//
//
//        const char* alphaModes[] = { "Opaque", "Mask", "Blend" };
//
//        changed |= ImGui::Combo("Alpha Mode", &material->alphaMode, alphaModes, IM_ARRAYSIZE(alphaModes));
//
//
//
//        ImGui::Spacing();
//
//
//
//        if (changed) {
//
//            MaterialPreviewStudio::Instance().RequestPreview(material);
//
//        }
//
//
//
//        if (ImGui::Button("Save Material")) {
//
//            material->Save();
//
//            ThumbnailGenerator::Instance().Invalidate(material->GetFilePath());
//
//        }
//
//        ImGui::SameLine();
//
//        ImGui::TextDisabled("Textures preview live, save manually.");
//
//
//
//        ImGui::Spacing();
//
//
//
//        auto& studio = MaterialPreviewStudio::Instance();
//
//        if (studio.IsReady()) {
//
//            static MaterialAsset* s_lastMaterial = nullptr;
//
//            if (changed || s_lastMaterial != material) {
//
//                studio.RequestPreview(material);
//
//                s_lastMaterial = material;
//
//            }
//
//            if (auto* tex = studio.GetPreviewTexture()) {
//
//                if (void* texId = ImGuiRenderer::GetTextureID(tex)) {
//
//                    float w = ImGui::GetContentRegionAvail().x;
//
//                    if (w > 256.0f) w = 256.0f;
//
//                    ImGui::Image((ImTextureID)texId, ImVec2(w, w));
//
//                }
//
//            }
//
//        }
//
//    }
//
//
//
//    void DrawAudioInspector(const std::string& path, const std::string& filename) {
//
//        ImGui::Text("Audio");
//
//        ImGui::TextWrapped("%s", filename.c_str());
//
//        ImGui::Separator();
//
//
//
//        std::error_code ec;
//
//        if (std::filesystem::exists(path, ec)) {
//
//            const auto bytes = std::filesystem::file_size(path, ec);
//
//            if (!ec) {
//
//                ImGui::Text("Size: %.2f KB", static_cast<double>(bytes) / 1024.0);
//
//            }
//
//        }
//
//
//
//        auto& audio = EngineKernel::Instance().GetAudioWorld();
//        const AudioClipAsset clip = audio.DescribeClip(path);
//        const bool previewing = audio.IsPreviewing(path);
//
//        if (ImGui::Button(previewing ? ICON_FA_STOP " Stop Preview" : ICON_FA_PLAY " Preview")) {
//
//            audio.TogglePreviewClip(path, AudioBusType::UI);
//
//        }
//
//        if (previewing) {
//
//            ImGui::SameLine();
//
//            ImGui::TextDisabled("Previewing");
//
//        }
//
//        float previewCursorSeconds = 0.0f;
//        float previewLengthSeconds = 0.0f;
//        const bool hasPreviewProgress = audio.GetPreviewPlaybackProgress(previewCursorSeconds, previewLengthSeconds) && previewLengthSeconds > 0.0f;
//        if (hasPreviewProgress) {
//            float seekSeconds = previewCursorSeconds;
//            if (ImGui::SliderFloat("##AudioPreviewSeek", &seekSeconds, 0.0f, previewLengthSeconds, "", ImGuiSliderFlags_NoInput)) {
//                audio.SeekPreview(seekSeconds);
//                previewCursorSeconds = seekSeconds;
//            }
//            ImGui::TextDisabled("%.2f / %.2f sec", previewCursorSeconds, previewLengthSeconds);
//        }
//
//        ImGui::Spacing();
//        ImGui::Separator();
//
//        if (clip.valid) {
//            ImGui::Text("Length: %.2f sec", clip.lengthSec);
//            ImGui::Text("Channels: %u", clip.channelCount);
//            ImGui::Text("Sample Rate: %u Hz", clip.sampleRate);
//            ImGui::Text("Streaming Default: %s", clip.streaming ? "Yes" : "No");
//        } else {
//            ImGui::TextDisabled("Metadata unavailable.");
//        }
//
//    }
//
//
//
//    void DrawMaterialInspector(const std::string& path, const std::string& filename) {
//
//        ImGui::Text("Material");
//
//        ImGui::TextWrapped("%s", filename.c_str());
//
//        ImGui::Separator();
//
//
//
//        auto material = ResourceManager::Instance().GetMaterial(path);
//
//        if (!material) {
//
//            ImGui::TextColored(ImVec4(1, 0, 0, 1), "Failed to load material.");
//
//            return;
//
//        }
//
//
//
//        DrawMaterialEditor(material.get());
//
//    }
//
//
//
//    std::string ReadAllText(const std::filesystem::path& path) {
//
//        std::ifstream ifs(path, std::ios::binary);
//
//        if (!ifs.is_open()) {
//
//            return {};
//
//        }
//
//        return std::string((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
//
//    }
//
//
//
//    EntitySnapshot::Snapshot BuildPrefabInstanceSnapshot(const std::filesystem::path& prefabPath) {
//
//        EntitySnapshot::Snapshot snapshot;
//
//        if (!PrefabSystem::LoadPrefabSnapshot(prefabPath, snapshot)) {
//
//            return snapshot;
//
//        }
//
//
//
//        for (auto& node : snapshot.nodes) {
//
//            if (node.localID != snapshot.rootLocalID) {
//
//                continue;
//
//            }
//
//
//
//            PrefabInstanceComponent prefabInstance;
//
//            prefabInstance.prefabAssetPath = prefabPath.generic_string();
//
//            prefabInstance.hasOverrides = false;
//
//            std::get<std::optional<PrefabInstanceComponent>>(node.components) = prefabInstance;
//
//            break;
//
//        }
//
//
//
//        return snapshot;
//
//    }
//
//
//
//    void DrawAssetInspector(const std::string& assetPath) {
//
//        std::filesystem::path path(assetPath);
//
//        const std::string filename = path.filename().string();
//
//        std::string ext = path.extension().string();
//
//        std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
//
//
//
//        if (ext == ".png" || ext == ".jpg" || ext == ".tga" || ext == ".dds" || ext == ".hdr") {
//
//            DrawTextureInspector(assetPath, filename);
//
//            return;
//
//        }
//
//        if (ext == ".fbx" || ext == ".obj" || ext == ".blend" || ext == ".gltf" || ext == ".cereal") {
//
//            DrawModelInspector(assetPath, filename);
//
//            return;
//
//        }
//
//        if (ext == ".mat") {
//
//            DrawMaterialInspector(assetPath, filename);
//
//            return;
//
//        }
//
//        if (ext == ".wav" || ext == ".ogg" || ext == ".mp3" || ext == ".flac") {
//
//            DrawAudioInspector(assetPath, filename);
//
//            return;
//
//        }
//
//
//
//        ImGui::TextWrapped("%s", filename.c_str());
//
//        ImGui::Separator();
//
//        ImGui::TextWrapped("Path: %s", assetPath.c_str());
//
//    }
//
//}
//
//
//
//void InspectorECSUI::Render(Registry* registry, bool* p_open, bool* outFocused) {
//
//    if (!ImGui::Begin(ICON_FA_CIRCLE_INFO " Inspector", p_open)) {
//
//        if (outFocused) {
//
//            *outFocused = false;
//
//        }
//
//        ImGui::End();
//
//        return;
//
//    }
//
//
//
//    if (outFocused) {
//
//        *outFocused = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);
//
//    }
//
//
//
//    auto& selection = EditorSelection::Instance();
//
//    switch (selection.GetType()) {
//
//    case SelectionType::Asset:
//
//        DrawAssetInspector(selection.GetAssetPath());
//
//        break;
//
//    case SelectionType::Entity:
//
//        if (!registry) {
//
//            ImGui::TextDisabled("Registry unavailable.");
//
//            break;
//
//        }
//
//        {
//
//            const EntityID entity = selection.GetEntity();
//
//            if (!registry->IsAlive(entity)) {
//                ImGui::TextDisabled("Entity is no longer available.");
//                break;
//            }
//
//            if (registry->GetComponent<SequencerPreviewCameraComponent>(entity)) {
//                ImGui::TextDisabled("Sequencer Camera");
//                ImGui::Separator();
//                ImGui::TextWrapped("This camera actor is generated and managed from Sequencer. Use the Scene view gizmo and Sequencer tracks to edit it.");
//                break;
//            }
//
//            if (registry->GetComponent<EffectPreviewTagComponent>(entity)) {
//                ImGui::TextDisabled("Preview Entity");
//                ImGui::Separator();
//                ImGui::TextWrapped("This entity is generated for editor preview only and is not part of the authored scene.");
//                break;
//            }
//
//            ImGui::Text("Entity");
//
//            ImGui::Separator();
//
//            ImGui::Text("Entity ID: %llu", static_cast<unsigned long long>(entity));
//
//
//
//            if (auto* name = registry->GetComponent<NameComponent>(entity)) {
//
//                ImGui::Text("Name: %s", name->name.c_str());
//
//            }
//
//
//
//            ImGui::Spacing();
//
//            DrawComponentIfPresent<NameComponent>(registry, entity);
//
//            DrawComponentIfPresent<TransformComponent>(registry, entity);
//
//            DrawComponentIfPresent<AudioEmitterComponent>(registry, entity);
//
//            DrawComponentIfPresent<AudioBusSendComponent>(registry, entity);
//
//            DrawComponentIfPresent<AudioListenerComponent>(registry, entity);
//
//            DrawComponentIfPresent<AudioSettingsComponent>(registry, entity);
//
//            DrawComponentIfPresent<RectTransformComponent>(registry, entity);
//
//            DrawComponentIfPresent<CanvasItemComponent>(registry, entity);
//
//            DrawComponentIfPresent<SpriteComponent>(registry, entity);
//
//            DrawComponentIfPresent<TextComponent>(registry, entity);
//
//            DrawComponentIfPresent<MeshComponent>(registry, entity);
//
//            if (auto* matComp = registry->GetComponent<MaterialComponent>(entity)) {
//
//                if (ImGui::CollapsingHeader("MaterialComponent", ImGuiTreeNodeFlags_DefaultOpen)) {
//
//                    char matBuf[512];
//
//                    strcpy_s(matBuf, matComp->materialAssetPath.c_str());
//
//                    const MaterialComponent beforeWidget = *matComp;
//
//                    if (ImGui::InputText("Material Path", matBuf, sizeof(matBuf))) {
//
//                        matComp->materialAssetPath = matBuf;
//
//                        if (!matComp->materialAssetPath.empty()) {
//
//                            matComp->materialAsset = ResourceManager::Instance().GetMaterial(matComp->materialAssetPath);
//
//                        } else {
//
//                            matComp->materialAsset = nullptr;
//
//                        }
//
//                    }
//
//                    static std::unordered_map<uint64_t, EditSession<MaterialComponent>> s_materialSessions;
//
//                    const uint64_t materialKey = MakeEditSessionKey<MaterialComponent>(entity);
//
//                    auto materialIt = s_materialSessions.find(materialKey);
//
//                    if ((ImGui::IsItemActivated() || ImGui::IsItemDeactivatedAfterEdit()) && materialIt == s_materialSessions.end()) {
//
//                        materialIt = s_materialSessions.emplace(materialKey, EditSession<MaterialComponent>{ beforeWidget, false }).first;
//
//                    }
//
//                    if (matComp->materialAssetPath != beforeWidget.materialAssetPath && materialIt != s_materialSessions.end()) {
//
//                        materialIt->second.dirty = true;
//
//                    }
//
//                    if (materialIt != s_materialSessions.end() && ImGui::IsItemDeactivatedAfterEdit()) {
//
//                        if (materialIt->second.dirty) {
//
//                            UndoSystem::Instance().ExecuteAction(
//
//                                std::make_unique<ComponentUndoAction<MaterialComponent>>(entity, materialIt->second.before, *matComp),
//
//                                *registry);
//
//                            PrefabSystem::MarkPrefabOverride(entity, *registry);
//
//                        }
//
//                        s_materialSessions.erase(materialIt);
//
//                    }
//
//                    if (matComp->materialAsset) {
//
//                        ImGui::Indent();
//
//                        DrawMaterialEditor(matComp->materialAsset.get());
//
//                        ImGui::Unindent();
//
//                    }
//
//                }
//
//            }
//
//            if (auto* prefabInstance = registry->GetComponent<PrefabInstanceComponent>(entity)) {
//
//                if (ImGui::CollapsingHeader("Prefab", ImGuiTreeNodeFlags_DefaultOpen)) {
//
//                    ImGui::TextWrapped("Source: %s", prefabInstance->prefabAssetPath.c_str());
//
//                    ImGui::Text("Overrides: %s", prefabInstance->hasOverrides ? "Yes" : "No");
//
//
//
//                    if (ImGui::Button("Apply Prefab")) {
//
//                        const std::filesystem::path prefabPath = prefabInstance->prefabAssetPath;
//
//                        const std::string beforeText = ReadAllText(prefabPath);
//
//                        const bool oldHasOverrides = prefabInstance->hasOverrides;
//
//                        if (PrefabSystem::ApplyPrefab(entity, *registry)) {
//
//                            const std::string afterText = ReadAllText(prefabPath);
//
//                            UndoSystem::Instance().RecordAction(
//
//                                std::make_unique<ApplyPrefabAction>(
//
//                                    entity,
//
//                                    prefabPath,
//
//                                    beforeText,
//
//                                    afterText,
//
//                                    oldHasOverrides,
//
//                                    prefabInstance->hasOverrides));
//
//                        }
//
//                    }
//
//                    ImGui::SameLine();
//
//                    if (ImGui::Button("Revert Prefab")) {
//
//                        EntityID parentEntity = Entity::NULL_ID;
//
//                        if (auto* hierarchy = registry->GetComponent<HierarchyComponent>(entity)) {
//
//                            parentEntity = hierarchy->parent;
//
//                        }
//
//
//
//                        EntitySnapshot::Snapshot beforeSnapshot = EntitySnapshot::CaptureSubtree(entity, *registry);
//
//                        EntitySnapshot::Snapshot afterSnapshot = BuildPrefabInstanceSnapshot(prefabInstance->prefabAssetPath);
//
//                        if (!beforeSnapshot.nodes.empty() && !afterSnapshot.nodes.empty()) {
//
//                            auto action = std::make_unique<ReplaceEntitySubtreeAction>(
//
//                                std::move(beforeSnapshot),
//
//                                std::move(afterSnapshot),
//
//                                entity,
//
//                                parentEntity,
//
//                                "Revert Prefab");
//
//                            auto* actionPtr = action.get();
//
//                            UndoSystem::Instance().ExecuteAction(std::move(action), *registry);
//
//                            EditorSelection::Instance().SelectEntity(actionPtr->GetLiveRoot());
//
//                        }
//
//                    }
//
//                    ImGui::SameLine();
//
//                    if (ImGui::Button("Unpack Prefab")) {
//
//                        UndoSystem::Instance().ExecuteAction(
//
//                            std::make_unique<OptionalComponentUndoAction<PrefabInstanceComponent>>(
//
//                                entity,
//
//                                std::optional<PrefabInstanceComponent>(*prefabInstance),
//
//                                std::nullopt,
//
//                                "Unpack Prefab"),
//
//                            *registry);
//
//                    }
//
//                }
//
//            }
//
//            DrawComponentIfPresent<HierarchyComponent>(registry, entity);
//
//            DrawComponentIfPresent<LightComponent>(registry, entity);
//
//            DrawComponentIfPresent<CameraFreeControlComponent>(registry, entity);
//
//            DrawComponentIfPresent<CameraLensComponent>(registry, entity);
//
//            DrawComponentIfPresent<Camera2DComponent>(registry, entity);
//
//            DrawComponentIfPresent<EnvironmentComponent>(registry, entity);
//
//            DrawComponentIfPresent<PostEffectComponent>(registry, entity);
//
//            DrawComponentIfPresent<ReflectionProbeComponent>(registry, entity);
//
//            DrawComponentIfPresent<ShadowSettingsComponent>(registry, entity);
//
//            // --- Gameplay Components (removable) ---
//            ImGui::Spacing();
//            DrawComponentRemovable<PlayerTagComponent>(registry, entity);
//            DrawComponentRemovable<CharacterPhysicsComponent>(registry, entity);
//            DrawComponentRemovable<HealthComponent>(registry, entity);
//            DrawComponentRemovable<StaminaComponent>(registry, entity);
//            DrawComponentRemovable<LocomotionStateComponent>(registry, entity);
//            DrawComponentRemovable<ActionStateComponent>(registry, entity);
//            DrawComponentRemovable<ActionDatabaseComponent>(registry, entity);
//            DrawComponentRemovable<DodgeStateComponent>(registry, entity);
//            DrawComponentRemovable<HitboxTrackingComponent>(registry, entity);
//            DrawComponentRemovable<StageBoundsComponent>(registry, entity);
//            DrawComponentRemovable<PlaybackComponent>(registry, entity);
//            DrawComponentRemovable<PlaybackRangeComponent>(registry, entity);
//            DrawComponentRemovable<TimelineComponent>(registry, entity);
//            DrawComponentRemovable<SpeedCurveComponent>(registry, entity);
//            DrawComponentRemovable<HitStopComponent>(registry, entity);
//            DrawComponentRemovable<EffectAssetComponent>(registry, entity);
//            DrawComponentRemovable<EffectPlaybackComponent>(registry, entity);
//            DrawComponentRemovable<EffectSpawnRequestComponent>(registry, entity);
//            DrawComponentRemovable<EffectAttachmentComponent>(registry, entity);
//            DrawComponentRemovable<EffectParameterOverrideComponent>(registry, entity);
//            DrawComponentRemovable<EffectPreviewTagComponent>(registry, entity);
//            DrawComponentRemovable<NodeAttachmentComponent>(registry, entity);
//            DrawComponentRemovable<NodeSocketComponent>(registry, entity);
//
//            // --- Input Components (removable) ---
//            DrawComponentRemovable<InputUserComponent>(registry, entity);
//            DrawComponentRemovable<InputContextComponent>(registry, entity);
//            DrawComponentRemovable<InputBindingComponent>(registry, entity);
//            DrawComponentRemovable<ResolvedInputStateComponent>(registry, entity);
//            DrawComponentRemovable<InputTextFieldComponent>(registry, entity);
//            DrawComponentRemovable<VibrationRequestComponent>(registry, entity);
//
//            // --- Add Component Button ---
//            ImGui::Spacing();
//            ImGui::Separator();
//            float width = ImGui::GetContentRegionAvail().x;
//            if (ImGui::Button("Add Component", ImVec2(width, 0))) {
//                ImGui::OpenPopup("AddComponentPopup");
//            }
//            if (ImGui::BeginPopup("AddComponentPopup")) {
//                ImGui::TextDisabled("-- Gameplay --");
//                TryAddComponent<PlayerTagComponent>(registry, entity, "PlayerTag");
//                TryAddComponent<CharacterPhysicsComponent>(registry, entity, "CharacterPhysics");
//                TryAddComponent<HealthComponent>(registry, entity, "Health");
//                TryAddComponent<StaminaComponent>(registry, entity, "Stamina");
//                TryAddComponent<LocomotionStateComponent>(registry, entity, "LocomotionState");
//                TryAddComponent<ActionStateComponent>(registry, entity, "ActionState");
//                TryAddComponent<ActionDatabaseComponent>(registry, entity, "ActionDatabase");
//                TryAddComponent<DodgeStateComponent>(registry, entity, "DodgeState");
//                TryAddComponent<HitboxTrackingComponent>(registry, entity, "HitboxTracking");
//                TryAddComponent<StageBoundsComponent>(registry, entity, "StageBounds");
//                TryAddComponent<PlaybackComponent>(registry, entity, "Playback");
//                TryAddComponent<PlaybackRangeComponent>(registry, entity, "PlaybackRange");
//                TryAddComponent<TimelineComponent>(registry, entity, "Timeline");
//                TryAddComponent<SpeedCurveComponent>(registry, entity, "SpeedCurve");
//                TryAddComponent<HitStopComponent>(registry, entity, "HitStop");
//                TryAddComponent<EffectAssetComponent>(registry, entity, "EffectAsset");
//                TryAddComponent<EffectPlaybackComponent>(registry, entity, "EffectPlayback");
//                TryAddComponent<EffectSpawnRequestComponent>(registry, entity, "EffectSpawnRequest");
//                TryAddComponent<EffectAttachmentComponent>(registry, entity, "EffectAttachment");
//                TryAddComponent<EffectParameterOverrideComponent>(registry, entity, "EffectParameterOverride");
//                TryAddComponent<EffectPreviewTagComponent>(registry, entity, "EffectPreviewTag");
//                TryAddComponent<NodeAttachmentComponent>(registry, entity, "NodeAttachment");
//                TryAddComponent<NodeSocketComponent>(registry, entity, "NodeSocket");
//                ImGui::Separator();
//                ImGui::TextDisabled("-- Input --");
//                TryAddComponent<InputUserComponent>(registry, entity, "InputUser");
//                TryAddComponent<InputContextComponent>(registry, entity, "InputContext");
//                TryAddComponent<InputBindingComponent>(registry, entity, "InputBinding");
//                TryAddComponent<ResolvedInputStateComponent>(registry, entity, "ResolvedInputState");
//                TryAddComponent<InputTextFieldComponent>(registry, entity, "InputTextField");
//                TryAddComponent<VibrationRequestComponent>(registry, entity, "VibrationRequest");
//                ImGui::Separator();
//                ImGui::TextDisabled("-- Core --");
//                TryAddComponent<ColliderComponent>(registry, entity, "Collider");
//                TryAddComponent<MeshComponent>(registry, entity, "Mesh");
//                TryAddComponent<LightComponent>(registry, entity, "Light");
//                TryAddComponent<AudioEmitterComponent>(registry, entity, "AudioEmitter");
//                TryAddComponent<AudioListenerComponent>(registry, entity, "AudioListener");
//                TryAddComponent<CameraLensComponent>(registry, entity, "CameraLens");
//                TryAddComponent<CameraFreeControlComponent>(registry, entity, "CameraFreeControl");
//                TryAddComponent<Camera2DComponent>(registry, entity, "Camera2D");
//                TryAddComponent<SpriteComponent>(registry, entity, "Sprite");
//                TryAddComponent<TextComponent>(registry, entity, "Text");
//                TryAddComponent<CanvasItemComponent>(registry, entity, "CanvasItem");
//                TryAddComponent<RectTransformComponent>(registry, entity, "RectTransform");
//                ImGui::EndPopup();
//            }
//
//        }
//
//        break;
//
//    default:
//
//        ImGui::TextDisabled("Nothing selected.");
//
//        break;
//
//    }
//
//
//
//    ImGui::End();
//
//}
//
#include "InspectorECSUI.h"

#include "Engine/EditorSelection.h"
#include "Engine/EngineKernel.h"
#include "System/ResourceManager.h"
#include "Font/FontManager.h"
#include "Material/MaterialPreviewStudio.h"
#include "Material/MaterialAsset.h"
#include "Asset/ThumbnailGenerator.h"
#include "Asset/PrefabSystem.h"
#include "Asset/AssetManager.h"
#include "ImGuiRenderer.h"
#include "Graphics.h"
#include "Generated/ComponentMeta.generated.h"
#include "Icon/IconsFontAwesome7.h"
#include "Component/NameComponent.h"

// Gameplay components
#include "Gameplay/PlayerTagComponent.h"
#include "Gameplay/CharacterPhysicsComponent.h"
#include "Gameplay/HealthComponent.h"
#include "Gameplay/StaminaComponent.h"
#include "Gameplay/LocomotionStateComponent.h"
#include "Gameplay/ActionStateComponent.h"
#include "Gameplay/ActionDatabaseComponent.h"
#include "Gameplay/DodgeStateComponent.h"
#include "Gameplay/HitboxTrackingComponent.h"
#include "Gameplay/StageBoundsComponent.h"
#include "Gameplay/PlaybackComponent.h"
#include "Gameplay/PlaybackRangeComponent.h"
#include "Gameplay/TimelineComponent.h"
#include "Gameplay/SpeedCurveComponent.h"
#include "Gameplay/HitStopComponent.h"
#include "Component/EffectAssetComponent.h"
#include "Component/EffectAttachmentComponent.h"
#include "Component/EffectParameterOverrideComponent.h"
#include "Component/EffectPlaybackComponent.h"
#include "Component/EffectPreviewTagComponent.h"
#include "Component/EffectSpawnRequestComponent.h"
#include "Component/SequencerPreviewCameraComponent.h"

// Input components
#include "Input/InputUserComponent.h"
#include "Input/InputContextComponent.h"
#include "Input/InputBindingComponent.h"
#include "Input/ResolvedInputStateComponent.h"
#include "Input/InputTextFieldComponent.h"
#include "Input/VibrationRequestComponent.h"

#include "Component/ColliderComponent.h"
#include "Component/TransformComponent.h"
#include "Component/MeshComponent.h"
#include "Component/LightComponent.h"
#include "Component/HierarchyComponent.h"
#include "Component/MaterialComponent.h"
#include "Component/PrefabInstanceComponent.h"
#include "Component/NodeAttachmentComponent.h"
#include "Component/NodeSocketComponent.h"
#include "Component/CameraComponent.h"
#include "Component/AudioEmitterComponent.h"
#include "Component/AudioBusSendComponent.h"
#include "Component/AudioListenerComponent.h"
#include "Component/AudioSettingsComponent.h"
#include "Component/Camera2DComponent.h"
#include "Component/CanvasItemComponent.h"
#include "Component/EnvironmentComponent.h"
#include "Component/PostEffectComponent.h"
#include "Component/RectTransformComponent.h"
#include "Component/ReflectionProbeComponent.h"
#include "Component/ShadowSettingsComponent.h"
#include "Component/SpriteComponent.h"
#include "Component/TextComponent.h"
#include "Component/UIButtonComponent.h"

#include "Engine/Editor2DEntityUtils.h"
#include "Hierarchy/HierarchySystem.h"
#include "Registry/Registry.h"
#include "System/UndoSystem.h"
#include "Undo/ComponentUndoAction.h"
#include "Undo/EntitySnapshot.h"
#include "Undo/EntityUndoActions.h"

#include <imgui.h>
#include <filesystem>
#include <algorithm>
#include <cctype>
#include <cfloat>
#include <cstring>
#include <fstream>
#include <memory>
#include <optional>
#include <type_traits>
#include <unordered_map>
#include <vector>

namespace {

    // ------------------------------------------------------------
    // shared_ptr 判定用 traits
    // ------------------------------------------------------------
    template <typename T>
    struct IsSharedPtr : std::false_type {};

    template <typename T>
    struct IsSharedPtr<std::shared_ptr<T>> : std::true_type {};

    // ------------------------------------------------------------
    // 汎用値描画ウィジェット
    // label と value を受け取り、型に応じて適切な ImGui 入力 UI を描く。
    // 値が変更されたら true を返す。
    // ------------------------------------------------------------
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

        }
        else if constexpr (std::is_same_v<T, float>) {
            changed = ImGui::DragFloat("##value", &value, 0.01f);

        }
        else if constexpr (std::is_same_v<T, int>) {
            changed = ImGui::DragInt("##value", &value, 1.0f);

        }
        else if constexpr (std::is_same_v<T, uint32_t>) {
            int temp = static_cast<int>(value);
            if (ImGui::DragInt("##value", &temp, 1.0f, 0)) {
                value = static_cast<uint32_t>(temp);
                changed = true;
            }

        }
        else if constexpr (std::is_same_v<T, std::string>) {
            char buffer[512] = {};
            strcpy_s(buffer, value.c_str());
            if (ImGui::InputText("##value", buffer, sizeof(buffer))) {
                value = buffer;
                changed = true;
            }

        }
        else if constexpr (std::is_same_v<T, DirectX::XMFLOAT2>) {
            changed = ImGui::DragFloat2("##value", &value.x, 0.01f);

        }
        else if constexpr (std::is_same_v<T, DirectX::XMFLOAT3>) {
            changed = ImGui::DragFloat3("##value", &value.x, 0.01f);

        }
        else if constexpr (std::is_same_v<T, DirectX::XMFLOAT4>) {
            changed = ImGui::DragFloat4("##value", &value.x, 0.01f);

        }
        else if constexpr (std::is_same_v<T, EntityID>) {
            ImGui::Text("%llu", static_cast<unsigned long long>(value));

        }
        else if constexpr (std::is_enum_v<T>) {
            int enumValue = static_cast<int>(value);
            if (ImGui::DragInt("##value", &enumValue, 1.0f, 0)) {
                value = static_cast<T>(enumValue);
                changed = true;
            }

        }
        else if constexpr (std::is_same_v<T, DirectX::XMFLOAT4X4>) {
            // 行列はここでは直接編集せず表示のみ。
            ImGui::TextDisabled("Matrix");

        }
        else if constexpr (IsSharedPtr<T>::value) {
            // shared_ptr はロード状態だけ簡易表示する。
            ImGui::TextDisabled(value ? "Loaded" : "None");

        }
        else {
            ImGui::TextDisabled("Unsupported");
        }

        ImGui::PopID();
        ImGui::TableNextColumn();
        return changed;
    }

    // ------------------------------------------------------------
    // Undo 用編集セッション
    // before 値と dirty 状態を保持する。
    // ------------------------------------------------------------
    template <typename T>
    struct EditSession {
        T before{};
        bool dirty = false;
    };

    // 型ごとの編集セッションテーブルを返す。
    template <typename T>
    std::unordered_map<uint64_t, EditSession<T>>& GetEditSessions() {
        static std::unordered_map<uint64_t, EditSession<T>> sessions;
        return sessions;
    }

    // EntityID からセッションキーを作る。
    template <typename T>
    uint64_t MakeEditSessionKey(EntityID entity) {
        return (static_cast<uint64_t>(Entity::GetIndex(entity)) << 32) | Entity::GetGeneration(entity);
    }

    std::unordered_map<uint64_t, RectTransformComponent>& GetTransformRectBeforeSessions()
    {
        static std::unordered_map<uint64_t, RectTransformComponent> sessions;
        return sessions;
    }

    // ------------------------------------------------------------
    // Undo 対応版の値描画ウィジェット
    // 編集開始時に before を記録し、編集終了時に UndoAction を積む。
    // ------------------------------------------------------------
    template <typename TComponent, typename TValue>
    bool DrawUndoableValueWidget(Registry* registry, EntityID entity, TComponent& component, const char* label, TValue& value) {
        const TComponent beforeWidget = component;
        const bool changed = DrawValueWidget(label, value);

        auto& sessions = GetEditSessions<TComponent>();
        const uint64_t key = MakeEditSessionKey<TComponent>(entity);
        auto it = sessions.find(key);

        // 編集開始または値変化時にセッションを開始する。
        if ((ImGui::IsItemActivated() || changed) && it == sessions.end()) {
            it = sessions.emplace(key, EditSession<TComponent>{ beforeWidget, false }).first;
            if constexpr (std::is_same_v<TComponent, TransformComponent>) {
                it->second.before.isDirty = true;
                if (registry) {
                    if (auto* rect = registry->GetComponent<RectTransformComponent>(entity)) {
                        GetTransformRectBeforeSessions()[key] = *rect;
                    }
                }
            }
        }

        // 値変化があれば dirty にする。
        if (changed && it != sessions.end()) {
            it->second.dirty = true;
            if constexpr (std::is_same_v<TComponent, TransformComponent>) {
                component.isDirty = true;
                if (registry) {
                    if (auto* rect = registry->GetComponent<RectTransformComponent>(entity)) {
                        Editor2D::SyncRectTransformFromTransform(component, *rect);
                    }
                    HierarchySystem::MarkDirtyRecursive(entity, *registry);
                    HierarchySystem hierarchySystem;
                    hierarchySystem.Update(*registry);
                }
            }
        }

        // 編集完了後に差分があれば Undo を積む。
        if (it != sessions.end() && ImGui::IsItemDeactivatedAfterEdit()) {
            if (it->second.dirty) {
                if constexpr (std::is_same_v<TComponent, TransformComponent>) {
                    component.isDirty = true;
                    auto composite = std::make_unique<CompositeUndoAction>("Transform Change");
                    composite->Add(std::make_unique<ComponentUndoAction<TComponent>>(entity, it->second.before, component));

                    auto& rectSessions = GetTransformRectBeforeSessions();
                    auto rectIt = rectSessions.find(key);
                    auto* rect = registry ? registry->GetComponent<RectTransformComponent>(entity) : nullptr;
                    if (rect && rectIt != rectSessions.end()) {
                        composite->Add(std::make_unique<ComponentUndoAction<RectTransformComponent>>(entity, rectIt->second, *rect));
                    }

                    UndoSystem::Instance().ExecuteAction(std::move(composite), *registry);
                    rectSessions.erase(key);
                } else {
                    UndoSystem::Instance().ExecuteAction(
                        std::make_unique<ComponentUndoAction<TComponent>>(entity, it->second.before, component),
                        *registry);
                }
                PrefabSystem::MarkPrefabOverride(entity, *registry);
            } else if constexpr (std::is_same_v<TComponent, TransformComponent>) {
                GetTransformRectBeforeSessions().erase(key);
            }
            sessions.erase(it);
        }

        return changed;
    }

    // ------------------------------------------------------------
    // Component の各 field を meta 情報から一括描画する。
    // ------------------------------------------------------------
    template <typename T>
    void DrawComponentBlock(Registry* registry, EntityID entity, const char* label, T& component) {
        if (!ImGui::CollapsingHeader(label, ImGuiTreeNodeFlags_DefaultOpen)) {
            return;
        }

        if (ImGui::BeginTable(label, 2, ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_BordersInnerV)) {
            ImGui::TableSetupColumn("Field", ImGuiTableColumnFlags_WidthFixed, 180.0f);
            ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableNextColumn();

            std::apply([&](auto... fields) {
                ((DrawUndoableValueWidget(registry, entity, component, fields.name.data(), component.*(fields.ptr))), ...);
                }, ComponentMeta<T>::Fields);

            ImGui::EndTable();
        }
    }

    // Component が存在する場合だけ描画する。
    template <typename T>
    void DrawComponentIfPresent(Registry* registry, EntityID entity) {
        if (T* component = registry->GetComponent<T>(entity)) {
            DrawComponentBlock(registry, entity, ComponentMeta<T>::Name.data(), *component);
        }
    }

    // ------------------------------------------------------------
    // 削除ボタン付き Component 描画
    // Header の X ボタンで RemoveComponent を行う。
    // ------------------------------------------------------------
    template <typename T>
    bool DrawComponentRemovable(Registry* registry, EntityID entity) {
        T* component = registry->GetComponent<T>(entity);
        if (!component) return false;

        const char* name = ComponentMeta<T>::Name.data();
        bool open = true;
        bool hdrOpen = ImGui::CollapsingHeader(name, &open, ImGuiTreeNodeFlags_DefaultOpen);

        if (!open) {
            // X ボタンが押されたら削除。
            registry->RemoveComponent<T>(entity);
            return true;
        }

        if (hdrOpen) {
            if (ImGui::BeginTable(name, 2, ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_BordersInnerV)) {
                ImGui::TableSetupColumn("Field", ImGuiTableColumnFlags_WidthFixed, 180.0f);
                ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);
                ImGui::TableNextColumn();

                std::apply([&](auto... fields) {
                    ((DrawUndoableValueWidget(registry, entity, *component, fields.name.data(), component->*(fields.ptr))), ...);
                    }, ComponentMeta<T>::Fields);

                ImGui::EndTable();
            }
        }
        return false;
    }

    // ------------------------------------------------------------
    // Add Component 用ヘルパー
    // 未所持ならメニューから追加できる。
    // ------------------------------------------------------------
    template <typename T>
    bool TryAddComponent(Registry* registry, EntityID entity, const char* label) {
        if (registry->GetComponent<T>(entity)) return false;
        if (ImGui::MenuItem(label)) {
            registry->AddComponent(entity, T{});
            return true;
        }
        return false;
    }

    // ------------------------------------------------------------
    // Texture asset inspector
    // ------------------------------------------------------------
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
            }
            else {
                ImGui::TextDisabled("Preview unavailable on this renderer.");
            }
        }
    }

    // ------------------------------------------------------------
    // Model asset inspector
    // ------------------------------------------------------------
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

    // ------------------------------------------------------------
    // テクスチャ選択ポップアップ用キャッシュ
    // AssetManager を再帰走査して texture 一覧を保持する。
    // ------------------------------------------------------------
    struct TexturePathCache {
        std::vector<std::string> paths;
        std::vector<std::string> filenames;
        bool initialized = false;

        void EnsureBuilt() {
            if (initialized) return;
            initialized = true;
            paths.clear();
            filenames.clear();
            CollectRecursive(AssetManager::Instance().GetRootDirectory());
        }

        void Invalidate() { initialized = false; }

    private:
        void CollectRecursive(const std::filesystem::path& dir) {
            auto entries = AssetManager::Instance().GetAssetsInDirectory(dir);

            for (auto& e : entries) {
                if (e.type == AssetType::Texture) {
                    paths.push_back(e.path.string());
                    filenames.push_back(e.path.filename().string());
                }
                else if (e.type == AssetType::Folder) {
                    CollectRecursive(e.path);
                }
            }
        }
    };

    static TexturePathCache s_texCache;

    // ------------------------------------------------------------
    // Material editor 用テクスチャスロット描画
    // D&D / ポップアップ選択 / クリアに対応。
    // ------------------------------------------------------------
    bool DrawTextureSlot(const char* label, std::string& texPath, bool allowPicker = true) {
        ImGui::PushID(label);

        bool changed = false;

        ImGui::Text("%s", label);
        ImGui::SameLine();

        const float thumbSize = 48.0f;

        // 現在割り当て中のテクスチャだけサムネイル表示する。
        ImVec2 pos = ImGui::GetCursorScreenPos();
        (void)pos;
        ImGui::InvisibleButton("##slot", ImVec2(thumbSize, thumbSize));
        bool clicked = ImGui::IsItemClicked(0);

        ImVec2 p0 = ImGui::GetItemRectMin();
        ImVec2 p1 = ImGui::GetItemRectMax();

        ImDrawList* drawList = ImGui::GetWindowDrawList();
        if (!texPath.empty()) {
            auto tex = ResourceManager::Instance().GetTexture(texPath);
            void* texId = tex ? ImGuiRenderer::GetTextureID(tex.get()) : nullptr;
            if (texId) {
                drawList->AddImage((ImTextureID)texId, p0, p1);
            }
            else {
                drawList->AddRectFilled(p0, p1, IM_COL32(60, 60, 60, 255));
                drawList->AddText(ImVec2(p0.x + 4, p0.y + 16), IM_COL32(255, 255, 0, 255), "?");
            }
        }
        else {
            drawList->AddRectFilled(p0, p1, IM_COL32(50, 50, 50, 255));
            drawList->AddRect(p0, p1, IM_COL32(100, 100, 100, 255));
        }
        drawList->AddRect(p0, p1, IM_COL32(120, 120, 120, 255));

        // Asset D&D に対応する。
        if (ImGui::BeginDragDropTarget()) {
            if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ENGINE_ASSET")) {
                std::string dropped((const char*)payload->Data);
                std::string ext = std::filesystem::path(dropped).extension().string();
                std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
                if (ext == ".png" || ext == ".jpg" || ext == ".tga" || ext == ".dds" || ext == ".hdr") {
                    texPath = dropped;
                    changed = true;
                }
            }
            ImGui::EndDragDropTarget();
        }

        // Material editor keeps the popup picker; SpriteComponent uses AssetBrowser D&D only.
        if (clicked && allowPicker) {
            s_texCache.EnsureBuilt();
            ImGui::OpenPopup("##texPicker");
        }

        // texture picker popup
        if (allowPicker && ImGui::BeginPopup("##texPicker")) {
            static char filterBuf[128] = "";
            ImGui::InputText("Filter", filterBuf, sizeof(filterBuf));

            ImGui::BeginChild("##texList", ImVec2(300, 200), true);
            for (size_t i = 0; i < s_texCache.paths.size(); ++i) {
                auto& fn = s_texCache.filenames[i];

                if (filterBuf[0] != '\0') {
                    std::string lower = fn;
                    std::string filterLower = filterBuf;
                    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
                    std::transform(filterLower.begin(), filterLower.end(), filterLower.begin(), ::tolower);
                    if (lower.find(filterLower) == std::string::npos) continue;
                }

                bool selected = (s_texCache.paths[i] == texPath);
                auto previewTexture = ResourceManager::Instance().GetTexture(s_texCache.paths[i]);
                void* previewId = previewTexture ? ImGuiRenderer::GetTextureID(previewTexture.get()) : nullptr;

                ImGui::PushID(static_cast<int>(i));

                if (previewId) {
                    ImGui::Image((ImTextureID)previewId, ImVec2(36.0f, 36.0f));
                }
                else {
                    ImGui::Dummy(ImVec2(36.0f, 36.0f));
                }

                ImGui::SameLine();
                if (ImGui::Selectable(fn.c_str(), selected, 0, ImVec2(0.0f, 36.0f))) {
                    texPath = s_texCache.paths[i];
                    changed = true;
                    filterBuf[0] = '\0';
                    ImGui::CloseCurrentPopup();
                    ImGui::PopID();
                    break;
                }

                ImGui::PopID();
            }
            ImGui::EndChild();

            if (ImGui::Button("None")) {
                texPath.clear();
                changed = true;
                filterBuf[0] = '\0';
                ImGui::CloseCurrentPopup();
            }

            ImGui::EndPopup();
        }

        ImGui::SameLine();
        if (!texPath.empty()) {
            std::string filename = std::filesystem::path(texPath).filename().string();
            ImGui::TextWrapped("%s", filename.c_str());
        }
        else {
            ImGui::TextDisabled("(empty)");
        }

        if (!texPath.empty()) {
            ImGui::SameLine();
            if (ImGui::SmallButton("X")) {
                texPath.clear();
                changed = true;
            }
        }

        ImGui::PopID();
        return changed;
    }

    template <typename T>
    void ExecuteImmediateComponentChange(Registry* registry,
                                         EntityID entity,
                                         const T& before,
                                         const T& after)
    {
        if (!registry) {
            return;
        }

        UndoSystem::Instance().ExecuteAction(
            std::make_unique<ComponentUndoAction<T>>(entity, before, after),
            *registry);
        PrefabSystem::MarkPrefabOverride(entity, *registry);
    }

    template <typename T>
    void ExecuteAddComponent(Registry* registry,
                             EntityID entity,
                             const T& value,
                             const char* actionName)
    {
        if (!registry || registry->GetComponent<T>(entity)) {
            return;
        }

        UndoSystem::Instance().ExecuteAction(
            std::make_unique<OptionalComponentUndoAction<T>>(
                entity,
                std::nullopt,
                std::optional<T>(value),
                actionName),
            *registry);
        PrefabSystem::MarkPrefabOverride(entity, *registry);
    }

    template <typename TComponent>
    bool DrawUndoableColorEdit4(Registry* registry,
                                EntityID entity,
                                TComponent& component,
                                const char* label,
                                DirectX::XMFLOAT4& value)
    {
        const TComponent beforeWidget = component;
        const bool changed = ImGui::ColorEdit4(label, &value.x);

        auto& sessions = GetEditSessions<TComponent>();
        const uint64_t key = MakeEditSessionKey<TComponent>(entity);
        auto it = sessions.find(key);

        if ((ImGui::IsItemActivated() || changed) && it == sessions.end()) {
            it = sessions.emplace(key, EditSession<TComponent>{ beforeWidget, false }).first;
        }

        if (changed && it != sessions.end()) {
            it->second.dirty = true;
        }

        if (it != sessions.end() && ImGui::IsItemDeactivatedAfterEdit()) {
            if (it->second.dirty) {
                ExecuteImmediateComponentChange(registry, entity, it->second.before, component);
            }
            sessions.erase(it);
        }

        return changed;
    }

    template <typename TComponent>
    bool DrawUndoableDragFloat(Registry* registry,
                               EntityID entity,
                               TComponent& component,
                               const char* label,
                               float& value,
                               float speed,
                               float minValue,
                               float maxValue,
                               const char* format = "%.3f")
    {
        const TComponent beforeWidget = component;
        bool changed = ImGui::DragFloat(label, &value, speed, minValue, maxValue, format);
        if (changed) {
            value = (std::clamp)(value, minValue, maxValue);
        }

        auto& sessions = GetEditSessions<TComponent>();
        const uint64_t key = MakeEditSessionKey<TComponent>(entity);
        auto it = sessions.find(key);

        if ((ImGui::IsItemActivated() || changed) && it == sessions.end()) {
            it = sessions.emplace(key, EditSession<TComponent>{ beforeWidget, false }).first;
        }

        if (changed && it != sessions.end()) {
            it->second.dirty = true;
        }

        if (it != sessions.end() && ImGui::IsItemDeactivatedAfterEdit()) {
            if (it->second.dirty) {
                ExecuteImmediateComponentChange(registry, entity, it->second.before, component);
            }
            sessions.erase(it);
        }

        return changed;
    }

    bool IsSupportedFontAssetPath(const std::string& path)
    {
        std::string ext = std::filesystem::path(path).extension().string();
        std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) {
            return static_cast<char>(std::tolower(c));
        });
        return ext == ".ttf" || ext == ".otf" || ext == ".fnt";
    }

    const char* TextAlignmentLabel(TextAlignment alignment)
    {
        switch (alignment) {
        case TextAlignment::Center:
            return "Center";
        case TextAlignment::Right:
            return "Right";
        case TextAlignment::Left:
        default:
            return "Left";
        }
    }

    int CountTextLines(const std::string& text)
    {
        if (text.empty()) {
            return 1;
        }

        int lines = 1;
        for (char c : text) {
            if (c == '\n') {
                ++lines;
            }
        }
        return lines;
    }

    ImVec2 MeasureTextComponentBounds(const TextComponent& text,
                                      const RectTransformComponent* rect)
    {
        ImFont* font = ImGui::GetFont();
        if (!text.fontAssetPath.empty()) {
            if (ImFont* previewFont = FontManager::Instance().GetEditorPreviewFont(text.fontAssetPath)) {
                font = previewFont;
            }
            else if (IsSupportedFontAssetPath(text.fontAssetPath)) {
                FontManager::Instance().QueueEditorPreviewFont(text.fontAssetPath, text.fontSize);
            }
        }

        const float fontSize = (std::max)(1.0f, text.fontSize);
        const float wrapWidth = (text.wrapping && rect)
            ? (std::max)(1.0f, rect->sizeDelta.x)
            : 0.0f;
        ImVec2 size = font->CalcTextSizeA(
            fontSize,
            FLT_MAX,
            wrapWidth,
            text.text.c_str());

        if (text.lineSpacing != 1.0f) {
            const int lineCount = CountTextLines(text.text);
            if (lineCount > 1) {
                size.y += static_cast<float>(lineCount - 1) * fontSize * (text.lineSpacing - 1.0f);
            }
        }

        size.x = (std::max)(1.0f, size.x);
        size.y = (std::max)(1.0f, size.y);
        return size;
    }

    bool DrawUndoableTextBody(Registry* registry,
                              EntityID entity,
                              TextComponent& component)
    {
        const TextComponent beforeWidget = component;
        const size_t bufferSize = (std::max)(
            static_cast<size_t>(4096),
            component.text.size() + static_cast<size_t>(1024));
        std::vector<char> buffer(bufferSize, '\0');
        strncpy_s(buffer.data(), buffer.size(), component.text.c_str(), _TRUNCATE);

        const int visibleLines = (std::clamp)(CountTextLines(component.text) + 1, 3, 8);
        const float height = (std::clamp)(
            ImGui::GetTextLineHeightWithSpacing() * static_cast<float>(visibleLines) + 18.0f,
            72.0f,
            160.0f);

        const bool changed = ImGui::InputTextMultiline(
            "##TextBody",
            buffer.data(),
            buffer.size(),
            ImVec2(-1.0f, height));

        auto& sessions = GetEditSessions<TextComponent>();
        const uint64_t key = MakeEditSessionKey<TextComponent>(entity);
        auto it = sessions.find(key);

        if ((ImGui::IsItemActivated() || changed) && it == sessions.end()) {
            it = sessions.emplace(key, EditSession<TextComponent>{ beforeWidget, false }).first;
        }

        if (changed && it != sessions.end()) {
            component.text = buffer.data();
            it->second.dirty = true;
        }

        if (it != sessions.end() && ImGui::IsItemDeactivatedAfterEdit()) {
            if (it->second.dirty) {
                ExecuteImmediateComponentChange(registry, entity, it->second.before, component);
            }
            sessions.erase(it);
        }

        return changed;
    }

    bool DrawFontSlot(const char* label, std::string& fontPath)
    {
        ImGui::PushID(label);
        ImGui::Text("%s", label);

        const std::string filename = fontPath.empty()
            ? std::string()
            : std::filesystem::path(fontPath).filename().string();
        const std::string slotLabel = fontPath.empty()
            ? std::string("Drop font asset here")
            : filename;

        ImGui::Button(slotLabel.c_str(), ImVec2(-1.0f, 38.0f));
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Drag .ttf, .otf, or .fnt from Asset Browser.");
        }

        bool changed = false;
        if (ImGui::BeginDragDropTarget()) {
            if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ENGINE_ASSET")) {
                std::string dropped(static_cast<const char*>(payload->Data));
                if (IsSupportedFontAssetPath(dropped)) {
                    fontPath = dropped;
                    changed = true;
                }
            }
            ImGui::EndDragDropTarget();
        }

        if (!fontPath.empty()) {
            ImGui::TextWrapped("%s", fontPath.c_str());
        }

        ImGui::PopID();
        return changed;
    }

    void DrawSpriteComponentInspector(Registry* registry, EntityID entity)
    {
        SpriteComponent* sprite = registry ? registry->GetComponent<SpriteComponent>(entity) : nullptr;
        if (!sprite) {
            return;
        }

        if (!ImGui::CollapsingHeader("SpriteComponent", ImGuiTreeNodeFlags_DefaultOpen)) {
            return;
        }

        TransformComponent* transform = registry->GetComponent<TransformComponent>(entity);
        RectTransformComponent* rect = registry->GetComponent<RectTransformComponent>(entity);
        CanvasItemComponent* canvas = registry->GetComponent<CanvasItemComponent>(entity);
        HierarchyComponent* hierarchy = registry->GetComponent<HierarchyComponent>(entity);

        bool hasBlockingIssue = false;
        if (!transform || !rect || !canvas) {
            hasBlockingIssue = true;
            ImGui::TextColored(ImVec4(1.0f, 0.74f, 0.28f, 1.0f), "Sprite preview needs Transform, RectTransform, and CanvasItem.");
        }
        if (hierarchy && !hierarchy->isActive) {
            hasBlockingIssue = true;
            ImGui::TextColored(ImVec4(1.0f, 0.74f, 0.28f, 1.0f), "Entity is inactive.");
        }
        if (canvas && !canvas->visible) {
            hasBlockingIssue = true;
            ImGui::TextColored(ImVec4(1.0f, 0.74f, 0.28f, 1.0f), "CanvasItem visible is off.");
        }
        if (sprite->textureAssetPath.empty()) {
            ImGui::TextDisabled("Texture is empty.");
        }

        if (hasBlockingIssue) {
            ImGui::Spacing();
        }

        bool requestRefresh = false;
        if (!transform) {
            if (ImGui::Button("Add Transform")) {
                TransformComponent value{};
                value.localScale = { 1.0f, 1.0f, 1.0f };
                value.isDirty = true;
                ExecuteAddComponent(registry, entity, value, "Add Transform");
                requestRefresh = true;
            }
            ImGui::SameLine();
        }
        if (!rect) {
            if (ImGui::Button("Add RectTransform")) {
                RectTransformComponent value{};
                value.sizeDelta = { 128.0f, 128.0f };
                ExecuteAddComponent(registry, entity, value, "Add RectTransform");
                requestRefresh = true;
            }
            ImGui::SameLine();
        }
        if (!canvas) {
            if (ImGui::Button("Add CanvasItem")) {
                ExecuteAddComponent(registry, entity, CanvasItemComponent{}, "Add CanvasItem");
                requestRefresh = true;
            }
            ImGui::SameLine();
        }
        if (requestRefresh) {
            return;
        }
        if (canvas && !canvas->visible) {
            if (ImGui::Button("Show CanvasItem")) {
                const CanvasItemComponent before = *canvas;
                canvas->visible = true;
                ExecuteImmediateComponentChange(registry, entity, before, *canvas);
            }
            ImGui::SameLine();
        }
        if (hierarchy && !hierarchy->isActive) {
            if (ImGui::Button("Activate Entity")) {
                const HierarchyComponent before = *hierarchy;
                hierarchy->isActive = true;
                ExecuteImmediateComponentChange(registry, entity, before, *hierarchy);
            }
            ImGui::SameLine();
        }
        if (hasBlockingIssue) {
            ImGui::NewLine();
            ImGui::Separator();
        }

        const SpriteComponent beforeTexture = *sprite;
        if (DrawTextureSlot("Texture", sprite->textureAssetPath, false) &&
            beforeTexture.textureAssetPath != sprite->textureAssetPath) {
            ExecuteImmediateComponentChange(registry, entity, beforeTexture, *sprite);
            Editor2D::FinalizeCreatedEntity(*registry, entity);
            return;
        }

        auto texture = sprite->textureAssetPath.empty()
            ? nullptr
            : ResourceManager::Instance().GetTexture(sprite->textureAssetPath);

        if (texture) {
            ImGui::TextDisabled("Texture Size: %u x %u",
                texture->GetWidth(),
                texture->GetHeight());
        }

        if (texture && rect) {
            if (ImGui::Button("Use Texture Size")) {
                const RectTransformComponent before = *rect;
                rect->sizeDelta = {
                    static_cast<float>(texture->GetWidth()),
                    static_cast<float>(texture->GetHeight())
                };
                ExecuteImmediateComponentChange(registry, entity, before, *rect);
                Editor2D::FinalizeCreatedEntity(*registry, entity);
                return;
            }
        }
        else if (!texture && !sprite->textureAssetPath.empty()) {
            ImGui::TextColored(ImVec4(1.0f, 0.42f, 0.42f, 1.0f), "Texture failed to load.");
        }

        DrawUndoableColorEdit4(registry, entity, *sprite, "Tint", sprite->tint);
    }

    void DrawTextComponentInspector(Registry* registry, EntityID entity)
    {
        TextComponent* text = registry ? registry->GetComponent<TextComponent>(entity) : nullptr;
        if (!text) {
            return;
        }

        if (!ImGui::CollapsingHeader("TextComponent", ImGuiTreeNodeFlags_DefaultOpen)) {
            return;
        }

        TransformComponent* transform = registry->GetComponent<TransformComponent>(entity);
        HierarchyComponent* hierarchy = registry->GetComponent<HierarchyComponent>(entity);
        RectTransformComponent* rect = registry->GetComponent<RectTransformComponent>(entity);
        CanvasItemComponent* canvas = registry->GetComponent<CanvasItemComponent>(entity);

        bool hasBlockingIssue = false;
        if (!transform || !hierarchy || !rect || !canvas) {
            hasBlockingIssue = true;
            ImGui::TextColored(ImVec4(1.0f, 0.74f, 0.28f, 1.0f), "Text preview needs Transform, Hierarchy, RectTransform, and CanvasItem.");
        }
        if (hierarchy && !hierarchy->isActive) {
            hasBlockingIssue = true;
            ImGui::TextColored(ImVec4(1.0f, 0.74f, 0.28f, 1.0f), "Entity is inactive.");
        }
        if (canvas && !canvas->visible) {
            hasBlockingIssue = true;
            ImGui::TextColored(ImVec4(1.0f, 0.74f, 0.28f, 1.0f), "CanvasItem visible is off.");
        }

        bool requestRefresh = false;
        if (!transform) {
            if (ImGui::Button("Add Transform")) {
                TransformComponent value{};
                value.localScale = { 1.0f, 1.0f, 1.0f };
                value.isDirty = true;
                ExecuteAddComponent(registry, entity, value, "Add Transform");
                requestRefresh = true;
            }
            ImGui::SameLine();
        }
        if (!hierarchy) {
            if (ImGui::Button("Add Hierarchy")) {
                ExecuteAddComponent(registry, entity, HierarchyComponent{}, "Add Hierarchy");
                requestRefresh = true;
            }
            ImGui::SameLine();
        }
        if (!rect) {
            if (ImGui::Button("Add RectTransform")) {
                RectTransformComponent value{};
                value.sizeDelta = { 320.0f, 80.0f };
                ExecuteAddComponent(registry, entity, value, "Add RectTransform");
                requestRefresh = true;
            }
            ImGui::SameLine();
        }
        if (!canvas) {
            if (ImGui::Button("Add CanvasItem")) {
                ExecuteAddComponent(registry, entity, CanvasItemComponent{}, "Add CanvasItem");
                requestRefresh = true;
            }
            ImGui::SameLine();
        }
        if (requestRefresh) {
            return;
        }

        if (canvas && !canvas->visible) {
            if (ImGui::Button("Show CanvasItem")) {
                const CanvasItemComponent before = *canvas;
                canvas->visible = true;
                ExecuteImmediateComponentChange(registry, entity, before, *canvas);
            }
            ImGui::SameLine();
        }
        if (hierarchy && !hierarchy->isActive) {
            if (ImGui::Button("Activate Entity")) {
                const HierarchyComponent before = *hierarchy;
                hierarchy->isActive = true;
                ExecuteImmediateComponentChange(registry, entity, before, *hierarchy);
            }
            ImGui::SameLine();
        }
        if (hasBlockingIssue) {
            ImGui::NewLine();
            ImGui::Separator();
        }

        ImGui::Text("Text");
        DrawUndoableTextBody(registry, entity, *text);

        ImGui::Spacing();
        const TextComponent beforeFont = *text;
        if (DrawFontSlot("Font", text->fontAssetPath) &&
            beforeFont.fontAssetPath != text->fontAssetPath) {
            ExecuteImmediateComponentChange(registry, entity, beforeFont, *text);
            FontManager::Instance().QueueEditorPreviewFont(text->fontAssetPath, text->fontSize);
            return;
        }

        if (text->fontAssetPath.empty()) {
            ImGui::TextDisabled("Font Status: default editor font.");
        }
        else if (!IsSupportedFontAssetPath(text->fontAssetPath)) {
            ImGui::TextColored(ImVec4(1.0f, 0.42f, 0.42f, 1.0f), "Font Status: unsupported font asset.");
        }
        else {
            std::error_code ec;
            const bool exists = std::filesystem::exists(text->fontAssetPath, ec);
            std::string ext = std::filesystem::path(text->fontAssetPath).extension().string();
            std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) {
                return static_cast<char>(std::tolower(c));
            });
            if (!exists) {
                ImGui::TextColored(ImVec4(1.0f, 0.42f, 0.42f, 1.0f), "Font Status: file not found.");
            }
            else if (ext == ".fnt") {
                ImGui::TextDisabled("Font Status: bitmap font assigned.");
            }
            else if (FontManager::Instance().GetEditorPreviewFont(text->fontAssetPath)) {
                ImGui::TextDisabled("Font Status: loaded.");
            }
            else {
                FontManager::Instance().QueueEditorPreviewFont(text->fontAssetPath, text->fontSize);
                ImGui::TextDisabled("Font Status: queued for preview.");
            }
        }

        DrawUndoableDragFloat(registry, entity, *text, "Font Size", text->fontSize, 1.0f, 1.0f, 512.0f, "%.1f");
        DrawUndoableColorEdit4(registry, entity, *text, "Color", text->color);

        const char* alignmentLabels[] = { "Left", "Center", "Right" };
        int alignmentIndex = static_cast<int>(text->alignment);
        alignmentIndex = (std::clamp)(alignmentIndex, 0, 2);
        const TextComponent beforeAlignment = *text;
        if (ImGui::Combo("Alignment", &alignmentIndex, alignmentLabels, IM_ARRAYSIZE(alignmentLabels))) {
            text->alignment = static_cast<TextAlignment>(alignmentIndex);
            ExecuteImmediateComponentChange(registry, entity, beforeAlignment, *text);
            return;
        }
        ImGui::TextDisabled("Current: %s", TextAlignmentLabel(text->alignment));

        const TextComponent beforeWrapping = *text;
        if (ImGui::Checkbox("Wrapping", &text->wrapping)) {
            ExecuteImmediateComponentChange(registry, entity, beforeWrapping, *text);
            return;
        }

        DrawUndoableDragFloat(registry, entity, *text, "Line Spacing", text->lineSpacing, 0.01f, 0.5f, 3.0f, "%.2f");

        if (rect && ImGui::Button("Use Text Size")) {
            const RectTransformComponent before = *rect;
            const ImVec2 bounds = MeasureTextComponentBounds(*text, rect);
            rect->sizeDelta = { bounds.x, bounds.y };
            ExecuteImmediateComponentChange(registry, entity, before, *rect);
            Editor2D::FinalizeCreatedEntity(*registry, entity);
            return;
        }
    }

    // ------------------------------------------------------------
    // MaterialAsset の直接編集 UI
    // ------------------------------------------------------------
    void DrawMaterialEditor(MaterialAsset* material) {
        if (!material) return;

        bool changed = false;

        const char* shaderNames[] = { "Phong", "PBR", "Toon" };
        int shaderIdx = material->shaderId;
        if (shaderIdx < 0 || shaderIdx > 2) shaderIdx = 1;

        if (ImGui::Combo("Shader", &shaderIdx, shaderNames, IM_ARRAYSIZE(shaderNames))) {
            material->shaderId = shaderIdx;
            changed = true;
        }

        changed |= ImGui::ColorEdit4("Base Color", &material->baseColor.x);
        changed |= ImGui::SliderFloat("Metallic", &material->metallic, 0.0f, 1.0f);
        changed |= ImGui::SliderFloat("Roughness", &material->roughness, 0.0f, 1.0f);
        changed |= ImGui::SliderFloat("Emissive", &material->emissive, 0.0f, 10.0f);

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Text("Textures");

        changed |= DrawTextureSlot("Diffuse", material->diffuseTexturePath);
        changed |= DrawTextureSlot("Normal", material->normalTexturePath);
        changed |= DrawTextureSlot("MetallicRoughness", material->metallicRoughnessTexturePath);
        changed |= DrawTextureSlot("Emissive", material->emissiveTexturePath);

        ImGui::Separator();

        const char* alphaModes[] = { "Opaque", "Mask", "Blend" };
        changed |= ImGui::Combo("Alpha Mode", &material->alphaMode, alphaModes, IM_ARRAYSIZE(alphaModes));

        ImGui::Spacing();

        // 変更があればプレビュー再生成要求を出す。
        if (changed) {
            MaterialPreviewStudio::Instance().RequestPreview(material);
        }

        // 保存時に material を書き出し、サムネイルも無効化する。
        if (ImGui::Button("Save Material")) {
            material->Save();
            ThumbnailGenerator::Instance().Invalidate(material->GetFilePath());
        }

        ImGui::SameLine();
        ImGui::TextDisabled("Textures preview live, save manually.");

        ImGui::Spacing();

        // 右側にプレビューを表示する。
        auto& studio = MaterialPreviewStudio::Instance();
        if (studio.IsReady()) {
            static MaterialAsset* s_lastMaterial = nullptr;

            if (changed || s_lastMaterial != material) {
                studio.RequestPreview(material);
                s_lastMaterial = material;
            }

            if (auto* tex = studio.GetPreviewTexture()) {
                if (void* texId = ImGuiRenderer::GetTextureID(tex)) {
                    float w = ImGui::GetContentRegionAvail().x;
                    if (w > 256.0f) w = 256.0f;
                    ImGui::Image((ImTextureID)texId, ImVec2(w, w));
                }
            }
        }
    }

    // ------------------------------------------------------------
    // Audio asset inspector
    // ------------------------------------------------------------
    void DrawAudioInspector(const std::string& path, const std::string& filename) {
        ImGui::Text("Audio");
        ImGui::TextWrapped("%s", filename.c_str());
        ImGui::Separator();

        std::error_code ec;
        if (std::filesystem::exists(path, ec)) {
            const auto bytes = std::filesystem::file_size(path, ec);
            if (!ec) {
                ImGui::Text("Size: %.2f KB", static_cast<double>(bytes) / 1024.0);
            }
        }

        auto& audio = EngineKernel::Instance().GetAudioWorld();
        const AudioClipAsset clip = audio.DescribeClip(path);
        const bool previewing = audio.IsPreviewing(path);

        if (ImGui::Button(previewing ? ICON_FA_STOP " Stop Preview" : ICON_FA_PLAY " Preview")) {
            audio.TogglePreviewClip(path, AudioBusType::UI);
        }

        if (previewing) {
            ImGui::SameLine();
            ImGui::TextDisabled("Previewing");
        }

        float previewCursorSeconds = 0.0f;
        float previewLengthSeconds = 0.0f;
        const bool hasPreviewProgress = audio.GetPreviewPlaybackProgress(previewCursorSeconds, previewLengthSeconds) && previewLengthSeconds > 0.0f;
        if (hasPreviewProgress) {
            float seekSeconds = previewCursorSeconds;
            if (ImGui::SliderFloat("##AudioPreviewSeek", &seekSeconds, 0.0f, previewLengthSeconds, "", ImGuiSliderFlags_NoInput)) {
                audio.SeekPreview(seekSeconds);
                previewCursorSeconds = seekSeconds;
            }
            ImGui::TextDisabled("%.2f / %.2f sec", previewCursorSeconds, previewLengthSeconds);
        }

        ImGui::Spacing();
        ImGui::Separator();

        if (clip.valid) {
            ImGui::Text("Length: %.2f sec", clip.lengthSec);
            ImGui::Text("Channels: %u", clip.channelCount);
            ImGui::Text("Sample Rate: %u Hz", clip.sampleRate);
            ImGui::Text("Streaming Default: %s", clip.streaming ? "Yes" : "No");
        }
        else {
            ImGui::TextDisabled("Metadata unavailable.");
        }
    }

    // Material asset inspector
    void DrawMaterialInspector(const std::string& path, const std::string& filename) {
        ImGui::Text("Material");
        ImGui::TextWrapped("%s", filename.c_str());
        ImGui::Separator();

        auto material = ResourceManager::Instance().GetMaterial(path);
        if (!material) {
            ImGui::TextColored(ImVec4(1, 0, 0, 1), "Failed to load material.");
            return;
        }

        DrawMaterialEditor(material.get());
    }

    // ファイル全文読み込み。
    std::string ReadAllText(const std::filesystem::path& path) {
        std::ifstream ifs(path, std::ios::binary);
        if (!ifs.is_open()) {
            return {};
        }
        return std::string((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
    }

    // prefab snapshot を読み込み、root に PrefabInstanceComponent を付けた形へ変換する。
    EntitySnapshot::Snapshot BuildPrefabInstanceSnapshot(const std::filesystem::path& prefabPath) {
        EntitySnapshot::Snapshot snapshot;
        if (!PrefabSystem::LoadPrefabSnapshot(prefabPath, snapshot)) {
            return snapshot;
        }

        for (auto& node : snapshot.nodes) {
            if (node.localID != snapshot.rootLocalID) {
                continue;
            }

            PrefabInstanceComponent prefabInstance;
            prefabInstance.prefabAssetPath = prefabPath.generic_string();
            prefabInstance.hasOverrides = false;
            std::get<std::optional<PrefabInstanceComponent>>(node.components) = prefabInstance;
            break;
        }

        return snapshot;
    }

    // ------------------------------------------------------------
    // Asset inspector のディスパッチ
    // 拡張子に応じて個別 inspector へ振り分ける。
    // ------------------------------------------------------------
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

        if (ext == ".wav" || ext == ".ogg" || ext == ".mp3" || ext == ".flac") {
            DrawAudioInspector(assetPath, filename);
            return;
        }

        // 未対応拡張子は簡易表示。
        ImGui::TextWrapped("%s", filename.c_str());
        ImGui::Separator();
        ImGui::TextWrapped("Path: %s", assetPath.c_str());
    }

} // namespace

// ------------------------------------------------------------
// Inspector ウィンドウ全体描画
// Asset 選択時は asset inspector、Entity 選択時は component inspector を表示する。
// ------------------------------------------------------------
void InspectorECSUI::Render(Registry* registry, bool* p_open, bool* outFocused) {
    if (!ImGui::Begin(ICON_FA_CIRCLE_INFO " Inspector", p_open)) {
        if (outFocused) {
            *outFocused = false;
        }
        ImGui::End();
        return;
    }

    if (outFocused) {
        *outFocused = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);
    }

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

            if (!registry->IsAlive(entity)) {
                ImGui::TextDisabled("Entity is no longer available.");
                break;
            }

            // Sequencer 管理カメラは専用メッセージ表示。
            if (registry->GetComponent<SequencerPreviewCameraComponent>(entity)) {
                ImGui::TextDisabled("Sequencer Camera");
                ImGui::Separator();
                ImGui::TextWrapped("This camera actor is generated and managed from Sequencer. Use the Scene view gizmo and Sequencer tracks to edit it.");
                break;
            }

            // Editor preview 専用 entity は編集不可メッセージを表示。
            if (registry->GetComponent<EffectPreviewTagComponent>(entity)) {
                ImGui::TextDisabled("Preview Entity");
                ImGui::Separator();
                ImGui::TextWrapped("This entity is generated for editor preview only and is not part of the authored scene.");
                break;
            }

            ImGui::Text("Entity");
            ImGui::Separator();
            ImGui::Text("Entity ID: %llu", static_cast<unsigned long long>(entity));

            if (auto* name = registry->GetComponent<NameComponent>(entity)) {
                ImGui::Text("Name: %s", name->name.c_str());
            }

            ImGui::Spacing();

            // --------------------------------------------------------
            // 基本 component 群
            // --------------------------------------------------------
            DrawComponentIfPresent<NameComponent>(registry, entity);
            DrawComponentIfPresent<TransformComponent>(registry, entity);
            DrawComponentIfPresent<AudioEmitterComponent>(registry, entity);
            DrawComponentIfPresent<AudioBusSendComponent>(registry, entity);
            DrawComponentIfPresent<AudioListenerComponent>(registry, entity);
            DrawComponentIfPresent<AudioSettingsComponent>(registry, entity);
            DrawComponentIfPresent<RectTransformComponent>(registry, entity);
            DrawComponentIfPresent<CanvasItemComponent>(registry, entity);
            DrawSpriteComponentInspector(registry, entity);
            DrawTextComponentInspector(registry, entity);
            DrawComponentIfPresent<UIButtonComponent>(registry, entity);
            DrawComponentIfPresent<MeshComponent>(registry, entity);

            // --------------------------------------------------------
            // MaterialComponent は専用エディタ付き
            // --------------------------------------------------------
            if (auto* matComp = registry->GetComponent<MaterialComponent>(entity)) {
                if (ImGui::CollapsingHeader("MaterialComponent", ImGuiTreeNodeFlags_DefaultOpen)) {
                    char matBuf[512];
                    strcpy_s(matBuf, matComp->materialAssetPath.c_str());

                    const MaterialComponent beforeWidget = *matComp;

                    if (ImGui::InputText("Material Path", matBuf, sizeof(matBuf))) {
                        matComp->materialAssetPath = matBuf;
                        if (!matComp->materialAssetPath.empty()) {
                            matComp->materialAsset = ResourceManager::Instance().GetMaterial(matComp->materialAssetPath);
                        }
                        else {
                            matComp->materialAsset = nullptr;
                        }
                    }

                    // MaterialComponent 専用 Undo セッション。
                    static std::unordered_map<uint64_t, EditSession<MaterialComponent>> s_materialSessions;
                    const uint64_t materialKey = MakeEditSessionKey<MaterialComponent>(entity);
                    auto materialIt = s_materialSessions.find(materialKey);

                    if ((ImGui::IsItemActivated() || ImGui::IsItemDeactivatedAfterEdit()) && materialIt == s_materialSessions.end()) {
                        materialIt = s_materialSessions.emplace(materialKey, EditSession<MaterialComponent>{ beforeWidget, false }).first;
                    }

                    if (matComp->materialAssetPath != beforeWidget.materialAssetPath && materialIt != s_materialSessions.end()) {
                        materialIt->second.dirty = true;
                    }

                    if (materialIt != s_materialSessions.end() && ImGui::IsItemDeactivatedAfterEdit()) {
                        if (materialIt->second.dirty) {
                            UndoSystem::Instance().ExecuteAction(
                                std::make_unique<ComponentUndoAction<MaterialComponent>>(entity, materialIt->second.before, *matComp),
                                *registry);
                            PrefabSystem::MarkPrefabOverride(entity, *registry);
                        }
                        s_materialSessions.erase(materialIt);
                    }

                    if (matComp->materialAsset) {
                        ImGui::Indent();
                        DrawMaterialEditor(matComp->materialAsset.get());
                        ImGui::Unindent();
                    }
                }
            }

            // --------------------------------------------------------
            // PrefabInstanceComponent は Apply / Revert / Unpack を持つ
            // --------------------------------------------------------
            if (auto* prefabInstance = registry->GetComponent<PrefabInstanceComponent>(entity)) {
                if (ImGui::CollapsingHeader("Prefab", ImGuiTreeNodeFlags_DefaultOpen)) {
                    ImGui::TextWrapped("Source: %s", prefabInstance->prefabAssetPath.c_str());
                    ImGui::Text("Overrides: %s", prefabInstance->hasOverrides ? "Yes" : "No");

                    // Apply
                    if (ImGui::Button("Apply Prefab")) {
                        const std::filesystem::path prefabPath = prefabInstance->prefabAssetPath;
                        const std::string beforeText = ReadAllText(prefabPath);
                        const bool oldHasOverrides = prefabInstance->hasOverrides;

                        if (PrefabSystem::ApplyPrefab(entity, *registry)) {
                            const std::string afterText = ReadAllText(prefabPath);
                            UndoSystem::Instance().RecordAction(
                                std::make_unique<ApplyPrefabAction>(
                                    entity,
                                    prefabPath,
                                    beforeText,
                                    afterText,
                                    oldHasOverrides,
                                    prefabInstance->hasOverrides));
                        }
                    }

                    ImGui::SameLine();

                    // Revert
                    if (ImGui::Button("Revert Prefab")) {
                        EntityID parentEntity = Entity::NULL_ID;
                        if (auto* hierarchy = registry->GetComponent<HierarchyComponent>(entity)) {
                            parentEntity = hierarchy->parent;
                        }

                        EntitySnapshot::Snapshot beforeSnapshot = EntitySnapshot::CaptureSubtree(entity, *registry);
                        EntitySnapshot::Snapshot afterSnapshot = BuildPrefabInstanceSnapshot(prefabInstance->prefabAssetPath);

                        if (!beforeSnapshot.nodes.empty() && !afterSnapshot.nodes.empty()) {
                            auto action = std::make_unique<ReplaceEntitySubtreeAction>(
                                std::move(beforeSnapshot),
                                std::move(afterSnapshot),
                                entity,
                                parentEntity,
                                "Revert Prefab");
                            auto* actionPtr = action.get();
                            UndoSystem::Instance().ExecuteAction(std::move(action), *registry);
                            EditorSelection::Instance().SelectEntity(actionPtr->GetLiveRoot());
                        }
                    }

                    ImGui::SameLine();

                    // Unpack
                    if (ImGui::Button("Unpack Prefab")) {
                        UndoSystem::Instance().ExecuteAction(
                            std::make_unique<OptionalComponentUndoAction<PrefabInstanceComponent>>(
                                entity,
                                std::optional<PrefabInstanceComponent>(*prefabInstance),
                                std::nullopt,
                                "Unpack Prefab"),
                            *registry);
                    }
                }
            }

            DrawComponentIfPresent<HierarchyComponent>(registry, entity);
            DrawComponentIfPresent<LightComponent>(registry, entity);
            DrawComponentIfPresent<CameraFreeControlComponent>(registry, entity);
            DrawComponentIfPresent<CameraLensComponent>(registry, entity);
            DrawComponentIfPresent<Camera2DComponent>(registry, entity);
            DrawComponentIfPresent<EnvironmentComponent>(registry, entity);
            DrawComponentIfPresent<PostEffectComponent>(registry, entity);
            DrawComponentIfPresent<ReflectionProbeComponent>(registry, entity);
            DrawComponentIfPresent<ShadowSettingsComponent>(registry, entity);

            // --------------------------------------------------------
            // Gameplay / Effect / NodeAttachment 系 removable components
            // --------------------------------------------------------
            ImGui::Spacing();
            DrawComponentRemovable<PlayerTagComponent>(registry, entity);
            DrawComponentRemovable<CharacterPhysicsComponent>(registry, entity);
            DrawComponentRemovable<HealthComponent>(registry, entity);
            DrawComponentRemovable<StaminaComponent>(registry, entity);
            DrawComponentRemovable<LocomotionStateComponent>(registry, entity);
            DrawComponentRemovable<ActionStateComponent>(registry, entity);
            DrawComponentRemovable<ActionDatabaseComponent>(registry, entity);
            DrawComponentRemovable<DodgeStateComponent>(registry, entity);
            DrawComponentRemovable<HitboxTrackingComponent>(registry, entity);
            DrawComponentRemovable<StageBoundsComponent>(registry, entity);
            DrawComponentRemovable<PlaybackComponent>(registry, entity);
            DrawComponentRemovable<PlaybackRangeComponent>(registry, entity);
            DrawComponentRemovable<TimelineComponent>(registry, entity);
            DrawComponentRemovable<SpeedCurveComponent>(registry, entity);
            DrawComponentRemovable<HitStopComponent>(registry, entity);
            DrawComponentRemovable<EffectAssetComponent>(registry, entity);
            DrawComponentRemovable<EffectPlaybackComponent>(registry, entity);
            DrawComponentRemovable<EffectSpawnRequestComponent>(registry, entity);
            DrawComponentRemovable<EffectAttachmentComponent>(registry, entity);
            DrawComponentRemovable<EffectParameterOverrideComponent>(registry, entity);
            DrawComponentRemovable<EffectPreviewTagComponent>(registry, entity);
            DrawComponentRemovable<NodeAttachmentComponent>(registry, entity);
            DrawComponentRemovable<NodeSocketComponent>(registry, entity);

            // --------------------------------------------------------
            // Input 系 removable components
            // --------------------------------------------------------
            DrawComponentRemovable<InputUserComponent>(registry, entity);
            DrawComponentRemovable<InputContextComponent>(registry, entity);
            DrawComponentRemovable<InputBindingComponent>(registry, entity);
            DrawComponentRemovable<ResolvedInputStateComponent>(registry, entity);
            DrawComponentRemovable<InputTextFieldComponent>(registry, entity);
            DrawComponentRemovable<VibrationRequestComponent>(registry, entity);

            // --------------------------------------------------------
            // Add Component ボタン
            // --------------------------------------------------------
            ImGui::Spacing();
            ImGui::Separator();

            float width = ImGui::GetContentRegionAvail().x;
            if (ImGui::Button("Add Component", ImVec2(width, 0))) {
                ImGui::OpenPopup("AddComponentPopup");
            }

            if (ImGui::BeginPopup("AddComponentPopup")) {
                ImGui::TextDisabled("-- Gameplay --");
                TryAddComponent<PlayerTagComponent>(registry, entity, "PlayerTag");
                TryAddComponent<CharacterPhysicsComponent>(registry, entity, "CharacterPhysics");
                TryAddComponent<HealthComponent>(registry, entity, "Health");
                TryAddComponent<StaminaComponent>(registry, entity, "Stamina");
                TryAddComponent<LocomotionStateComponent>(registry, entity, "LocomotionState");
                TryAddComponent<ActionStateComponent>(registry, entity, "ActionState");
                TryAddComponent<ActionDatabaseComponent>(registry, entity, "ActionDatabase");
                TryAddComponent<DodgeStateComponent>(registry, entity, "DodgeState");
                TryAddComponent<HitboxTrackingComponent>(registry, entity, "HitboxTracking");
                TryAddComponent<StageBoundsComponent>(registry, entity, "StageBounds");
                TryAddComponent<PlaybackComponent>(registry, entity, "Playback");
                TryAddComponent<PlaybackRangeComponent>(registry, entity, "PlaybackRange");
                TryAddComponent<TimelineComponent>(registry, entity, "Timeline");
                TryAddComponent<SpeedCurveComponent>(registry, entity, "SpeedCurve");
                TryAddComponent<HitStopComponent>(registry, entity, "HitStop");
                TryAddComponent<EffectAssetComponent>(registry, entity, "EffectAsset");
                TryAddComponent<EffectPlaybackComponent>(registry, entity, "EffectPlayback");
                TryAddComponent<EffectSpawnRequestComponent>(registry, entity, "EffectSpawnRequest");
                TryAddComponent<EffectAttachmentComponent>(registry, entity, "EffectAttachment");
                TryAddComponent<EffectParameterOverrideComponent>(registry, entity, "EffectParameterOverride");
                TryAddComponent<EffectPreviewTagComponent>(registry, entity, "EffectPreviewTag");
                TryAddComponent<NodeAttachmentComponent>(registry, entity, "NodeAttachment");
                TryAddComponent<NodeSocketComponent>(registry, entity, "NodeSocket");

                ImGui::Separator();
                ImGui::TextDisabled("-- Input --");
                TryAddComponent<InputUserComponent>(registry, entity, "InputUser");
                TryAddComponent<InputContextComponent>(registry, entity, "InputContext");
                TryAddComponent<InputBindingComponent>(registry, entity, "InputBinding");
                TryAddComponent<ResolvedInputStateComponent>(registry, entity, "ResolvedInputState");
                TryAddComponent<InputTextFieldComponent>(registry, entity, "InputTextField");
                TryAddComponent<VibrationRequestComponent>(registry, entity, "VibrationRequest");

                ImGui::Separator();
                ImGui::TextDisabled("-- Core --");
                TryAddComponent<ColliderComponent>(registry, entity, "Collider");
                TryAddComponent<MeshComponent>(registry, entity, "Mesh");
                TryAddComponent<LightComponent>(registry, entity, "Light");
                TryAddComponent<AudioEmitterComponent>(registry, entity, "AudioEmitter");
                TryAddComponent<AudioListenerComponent>(registry, entity, "AudioListener");
                TryAddComponent<CameraLensComponent>(registry, entity, "CameraLens");
                TryAddComponent<CameraFreeControlComponent>(registry, entity, "CameraFreeControl");
                TryAddComponent<Camera2DComponent>(registry, entity, "Camera2D");
                TryAddComponent<SpriteComponent>(registry, entity, "Sprite");
                TryAddComponent<TextComponent>(registry, entity, "Text");
                TryAddComponent<UIButtonComponent>(registry, entity, "UIButton");
                TryAddComponent<CanvasItemComponent>(registry, entity, "CanvasItem");
                TryAddComponent<RectTransformComponent>(registry, entity, "RectTransform");

                ImGui::EndPopup();
            }
        }
        break;

    default:
        ImGui::TextDisabled("Nothing selected.");
        break;
    }

    ImGui::End();
}
