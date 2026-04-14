#include "InspectorECSUI.h"

#include "Engine/EditorSelection.h"

#include "Engine/EngineKernel.h"

#include "System/ResourceManager.h"

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

#include "Registry/Registry.h"

#include "System/UndoSystem.h"

#include "Undo/ComponentUndoAction.h"

#include "Undo/EntitySnapshot.h"

#include "Undo/EntityUndoActions.h"

#include <imgui.h>

#include <filesystem>

#include <algorithm>

#include <fstream>

#include <memory>

#include <type_traits>

#include <unordered_map>



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

        } else if constexpr (std::is_same_v<T, DirectX::XMFLOAT2>) {

            changed = ImGui::DragFloat2("##value", &value.x, 0.01f);

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

    struct EditSession {

        T before{};

        bool dirty = false;

    };



    template <typename T>

    std::unordered_map<uint64_t, EditSession<T>>& GetEditSessions() {

        static std::unordered_map<uint64_t, EditSession<T>> sessions;

        return sessions;

    }



    template <typename T>

    uint64_t MakeEditSessionKey(EntityID entity) {

        return (static_cast<uint64_t>(Entity::GetIndex(entity)) << 32) | Entity::GetGeneration(entity);

    }



    template <typename TComponent, typename TValue>

    bool DrawUndoableValueWidget(Registry* registry, EntityID entity, TComponent& component, const char* label, TValue& value) {

        const TComponent beforeWidget = component;

        const bool changed = DrawValueWidget(label, value);



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

                UndoSystem::Instance().ExecuteAction(

                    std::make_unique<ComponentUndoAction<TComponent>>(entity, it->second.before, component),

                    *registry);

                PrefabSystem::MarkPrefabOverride(entity, *registry);

            }

            sessions.erase(it);

        }



        return changed;

    }



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



    template <typename T>

    void DrawComponentIfPresent(Registry* registry, EntityID entity) {

        if (T* component = registry->GetComponent<T>(entity)) {

            DrawComponentBlock(registry, entity, ComponentMeta<T>::Name.data(), *component);

        }

    }

    // Removable version - shows X button in header
    template <typename T>
    bool DrawComponentRemovable(Registry* registry, EntityID entity) {
        T* component = registry->GetComponent<T>(entity);
        if (!component) return false;

        const char* name = ComponentMeta<T>::Name.data();
        bool open = true;
        bool hdrOpen = ImGui::CollapsingHeader(name, &open, ImGuiTreeNodeFlags_DefaultOpen);

        if (!open) {
            // X button clicked — remove component
            registry->RemoveComponent<T>(entity);
            return true; // component removed
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

    // Add Component helper - returns true if added
    template <typename T>
    bool TryAddComponent(Registry* registry, EntityID entity, const char* label) {
        if (registry->GetComponent<T>(entity)) return false; // already present
        if (ImGui::MenuItem(label)) {
            registry->AddComponent(entity, T{});
            return true;
        }
        return false;
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



    // Cached texture path list (filenames only, no GPU load)

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

                } else if (e.type == AssetType::Folder) {

                    CollectRecursive(e.path);

                }

            }

        }

    };

    static TexturePathCache s_texCache;



    bool DrawTextureSlot(const char* label, std::string& texPath) {

        ImGui::PushID(label);

        bool changed = false;



        ImGui::Text("%s", label);

        ImGui::SameLine();



        const float thumbSize = 48.0f;



        // Thumbnail (show only the currently assigned texture, not all textures)

        ImVec2 pos = ImGui::GetCursorScreenPos();

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

            } else {

                drawList->AddRectFilled(p0, p1, IM_COL32(60, 60, 60, 255));

                drawList->AddText(ImVec2(p0.x + 4, p0.y + 16), IM_COL32(255, 255, 0, 255), "?");

            }

        } else {

            drawList->AddRectFilled(p0, p1, IM_COL32(50, 50, 50, 255));

            drawList->AddRect(p0, p1, IM_COL32(100, 100, 100, 255));

        }

        drawList->AddRect(p0, p1, IM_COL32(120, 120, 120, 255));



        // D&D target

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



        // Click to open texture picker popup

        if (clicked) {

            s_texCache.EnsureBuilt();

            ImGui::OpenPopup("##texPicker");

        }



        if (ImGui::BeginPopup("##texPicker")) {

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

                } else {

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

        } else {

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



        if (changed) {

            MaterialPreviewStudio::Instance().RequestPreview(material);

        }



        if (ImGui::Button("Save Material")) {

            material->Save();

            ThumbnailGenerator::Instance().Invalidate(material->GetFilePath());

        }

        ImGui::SameLine();

        ImGui::TextDisabled("Textures preview live, save manually.");



        ImGui::Spacing();



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
        } else {
            ImGui::TextDisabled("Metadata unavailable.");
        }

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



        DrawMaterialEditor(material.get());

    }



    std::string ReadAllText(const std::filesystem::path& path) {

        std::ifstream ifs(path, std::ios::binary);

        if (!ifs.is_open()) {

            return {};

        }

        return std::string((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());

    }



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



        ImGui::TextWrapped("%s", filename.c_str());

        ImGui::Separator();

        ImGui::TextWrapped("Path: %s", assetPath.c_str());

    }

}



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

            if (registry->GetComponent<SequencerPreviewCameraComponent>(entity)) {
                ImGui::TextDisabled("Sequencer Camera");
                ImGui::Separator();
                ImGui::TextWrapped("This camera actor is generated and managed from Sequencer. Use the Scene view gizmo and Sequencer tracks to edit it.");
                break;
            }

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

            DrawComponentIfPresent<NameComponent>(registry, entity);

            DrawComponentIfPresent<TransformComponent>(registry, entity);

            DrawComponentIfPresent<AudioEmitterComponent>(registry, entity);

            DrawComponentIfPresent<AudioBusSendComponent>(registry, entity);

            DrawComponentIfPresent<AudioListenerComponent>(registry, entity);

            DrawComponentIfPresent<AudioSettingsComponent>(registry, entity);

            DrawComponentIfPresent<RectTransformComponent>(registry, entity);

            DrawComponentIfPresent<CanvasItemComponent>(registry, entity);

            DrawComponentIfPresent<SpriteComponent>(registry, entity);

            DrawComponentIfPresent<TextComponent>(registry, entity);

            DrawComponentIfPresent<MeshComponent>(registry, entity);

            if (auto* matComp = registry->GetComponent<MaterialComponent>(entity)) {

                if (ImGui::CollapsingHeader("MaterialComponent", ImGuiTreeNodeFlags_DefaultOpen)) {

                    char matBuf[512];

                    strcpy_s(matBuf, matComp->materialAssetPath.c_str());

                    const MaterialComponent beforeWidget = *matComp;

                    if (ImGui::InputText("Material Path", matBuf, sizeof(matBuf))) {

                        matComp->materialAssetPath = matBuf;

                        if (!matComp->materialAssetPath.empty()) {

                            matComp->materialAsset = ResourceManager::Instance().GetMaterial(matComp->materialAssetPath);

                        } else {

                            matComp->materialAsset = nullptr;

                        }

                    }

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

            if (auto* prefabInstance = registry->GetComponent<PrefabInstanceComponent>(entity)) {

                if (ImGui::CollapsingHeader("Prefab", ImGuiTreeNodeFlags_DefaultOpen)) {

                    ImGui::TextWrapped("Source: %s", prefabInstance->prefabAssetPath.c_str());

                    ImGui::Text("Overrides: %s", prefabInstance->hasOverrides ? "Yes" : "No");



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

            // --- Gameplay Components (removable) ---
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

            // --- Input Components (removable) ---
            DrawComponentRemovable<InputUserComponent>(registry, entity);
            DrawComponentRemovable<InputContextComponent>(registry, entity);
            DrawComponentRemovable<InputBindingComponent>(registry, entity);
            DrawComponentRemovable<ResolvedInputStateComponent>(registry, entity);
            DrawComponentRemovable<InputTextFieldComponent>(registry, entity);
            DrawComponentRemovable<VibrationRequestComponent>(registry, entity);

            // --- Add Component Button ---
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

