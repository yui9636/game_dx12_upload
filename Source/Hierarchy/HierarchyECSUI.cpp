#include "HierarchyECSUI.h"
#include "Engine/EngineKernel.h"
#include "Registry/Registry.h"
#include "Engine/EditorSelection.h"
#include "Icon/IconFontManager.h"
#include "Hierarchy/HierarchySystem.h"
#include "Asset/PrefabSystem.h"
#include "Console/Logger.h"
#include "System/UndoSystem.h"
#include "Undo/ComponentUndoAction.h"
#include "Undo/EntityUndoActions.h"

#include "Component/NameComponent.h"
#include "Component/TransformComponent.h"
#include "Component/HierarchyComponent.h"
#include "Input/InputUserComponent.h"
#include "Input/InputContextComponent.h"
#include "Input/InputBindingComponent.h"
#include "Input/ResolvedInputStateComponent.h"
#include "Input/InputTextFieldComponent.h"
#include "Input/VibrationRequestComponent.h"
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
#include "Gameplay/TimelineItemBuffer.h"
#include "Gameplay/SpeedCurveComponent.h"
#include "Gameplay/HitStopComponent.h"
#include "Gameplay/PlayerRuntimeSetup.h"
#include "Component/MeshComponent.h"
#include "Component/LightComponent.h"
#include "Component/AudioEmitterComponent.h"
#include "Component/AudioSettingsComponent.h"
#include "Component/AudioListenerComponent.h"
#include "Component/Camera2DComponent.h"
#include "Component/CanvasItemComponent.h"
#include "Component/RectTransformComponent.h"
#include "Component/ReflectionProbeComponent.h"
#include "Component/EffectPreviewTagComponent.h"
#include "Component/SequencerPreviewCameraComponent.h"
#include "Component/SpriteComponent.h"
#include "Component/TextComponent.h"
#include "Component/UIButtonComponent.h"
#include "Component/EnvironmentComponent.h"

#include "System/ResourceManager.h"
#include "Model/Model.h"

#include <imgui.h>
#include <string>
#include <filesystem>
#include <algorithm>
#include <array>
#include <cctype>
#include <Component\MaterialComponent.h>

namespace
{
    // Hierarchy 検索用の小文字文字列。
    std::string s_hierarchyFilterLower;

    // 次フレームで検索ボックスへフォーカス要求を出す。
    bool s_requestHierarchySearchFocus = false;

    // 2D Text の既定フォント。
    constexpr const char* kDefault2DFontAssetPath = "Data/Font/ArialUni.ttf";

    // 文字列を小文字化したコピーを返す。
    std::string ToLowerCopy(std::string value)
    {
        std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
            return static_cast<char>(std::tolower(c));
            });
        return value;
    }

    // Entity の表示名を返す。
    // NameComponent が無ければ index ベースの仮名を返す。
    std::string GetEntityDisplayName(Registry& registry, EntityID entity)
    {
        if (auto* name = registry.GetComponent<NameComponent>(entity)) {
            return name->name;
        }
        return "Entity_" + std::to_string(Entity::GetIndex(entity));
    }

    // 現在の検索フィルタに entity 単体が一致するか判定する。
    bool EntityMatchesFilter(Registry& registry, EntityID entity)
    {
        if (s_hierarchyFilterLower.empty()) {
            return true;
        }
        return ToLowerCopy(GetEntityDisplayName(registry, entity)).find(s_hierarchyFilterLower) != std::string::npos;
    }

    // entity 自身または子孫のどれかが検索フィルタに一致するか判定する。
    // 一部の内部用 entity は Hierarchy 表示対象から除外する。
    bool SubtreeMatchesFilter(Registry& registry, EntityID entity)
    {
        if (registry.GetComponent<EnvironmentComponent>(entity) ||
            registry.GetComponent<ReflectionProbeComponent>(entity) ||
            registry.GetComponent<EffectPreviewTagComponent>(entity) ||
            registry.GetComponent<SequencerPreviewCameraComponent>(entity)) {
            return false;
        }

        if (s_hierarchyFilterLower.empty()) {
            return true;
        }

        if (EntityMatchesFilter(registry, entity)) {
            return true;
        }

        const HierarchyComponent* hierarchy = registry.GetComponent<HierarchyComponent>(entity);
        EntityID child = hierarchy ? hierarchy->firstChild : Entity::NULL_ID;
        while (!Entity::IsNull(child)) {
            if (SubtreeMatchesFilter(registry, child)) {
                return true;
            }
            hierarchy = registry.GetComponent<HierarchyComponent>(child);
            child = hierarchy ? hierarchy->nextSibling : Entity::NULL_ID;
        }
        return false;
    }

    // entity の親方向にたどって、既に選択済み ancestor がいるか判定する。
    bool HasSelectedAncestor(EntityID entity, Registry& registry, const EditorSelection& selection)
    {
        const HierarchyComponent* hierarchy = registry.GetComponent<HierarchyComponent>(entity);
        EntityID parent = hierarchy ? hierarchy->parent : Entity::NULL_ID;
        while (!Entity::IsNull(parent)) {
            if (selection.IsEntitySelected(parent)) {
                return true;
            }
            hierarchy = registry.GetComponent<HierarchyComponent>(parent);
            parent = hierarchy ? hierarchy->parent : Entity::NULL_ID;
        }
        return false;
    }

    // 現在選択中 entity のうち、祖先が同時選択されていない root 群だけを返す。
    std::vector<EntityID> GetSelectedRootEntities(Registry& registry, const EditorSelection& selection)
    {
        std::vector<EntityID> roots;
        for (EntityID entity : selection.GetSelectedEntities()) {
            if (!registry.IsAlive(entity)) {
                continue;
            }
            if (!HasSelectedAncestor(entity, registry, selection)) {
                roots.push_back(entity);
            }
        }
        return roots;
    }

    // Hierarchy 上で entity がクリックされた時の選択処理。
    // Ctrl 押下中はトグル、それ以外は単独選択にする。
    void HandleEntitySelectionClick(EntityID entity)
    {
        auto& selection = EditorSelection::Instance();
        ImGuiIO& io = ImGui::GetIO();
        if (io.KeyCtrl) {
            selection.ToggleEntity(entity, true);
        }
        else {
            selection.SelectEntity(entity);
        }
    }

    // Name / Transform / Hierarchy を持つ単体 entity 用 snapshot を作る。
    // 任意で Mesh / Light / ReflectionProbe も追加できる。
    EntitySnapshot::Snapshot BuildSingleEntitySnapshot(const std::string& name,
        const MeshComponent* meshComponent = nullptr,
        const LightComponent* lightComponent = nullptr,
        const ReflectionProbeComponent* probeComponent = nullptr)
    {
        EntitySnapshot::Snapshot snapshot;
        snapshot.rootLocalID = 0;

        EntitySnapshot::Node node;
        node.localID = 0;
        node.sourceEntity = Entity::NULL_ID;
        node.parentLocalID = EntitySnapshot::kInvalidLocalID;
        node.externalParent = Entity::NULL_ID;

        std::get<std::optional<NameComponent>>(node.components) = NameComponent{ name };
        std::get<std::optional<TransformComponent>>(node.components) = TransformComponent{};
        std::get<std::optional<HierarchyComponent>>(node.components) = HierarchyComponent{};
        if (meshComponent) {
            std::get<std::optional<MeshComponent>>(node.components) = *meshComponent;
        }
        if (lightComponent) {
            std::get<std::optional<LightComponent>>(node.components) = *lightComponent;
        }
        if (probeComponent) {
            std::get<std::optional<ReflectionProbeComponent>>(node.components) = *probeComponent;
        }

        snapshot.nodes.push_back(std::move(node));
        return snapshot;
    }

    // モデルとして扱えるアセット拡張子か判定する。
    bool IsSupportedModelAsset(const std::string& path)
    {
        std::string ext = std::filesystem::path(path).extension().string();
        std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
        return ext == ".fbx" || ext == ".obj" || ext == ".blend" || ext == ".gltf";
    }

    // スプライト用テクスチャとして扱える拡張子か判定する。
    bool IsSupportedSpriteAsset(const std::string& path)
    {
        std::string ext = std::filesystem::path(path).extension().string();
        std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
        return ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".tga" || ext == ".dds" || ext == ".bmp";
    }

    // フォントとして扱える拡張子か判定する。
    bool IsSupportedFontAsset(const std::string& path)
    {
        std::string ext = std::filesystem::path(path).extension().string();
        std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
        return ext == ".ttf" || ext == ".otf" || ext == ".fnt";
    }

    // オーディオクリップとして扱える拡張子か判定する。
    bool IsSupportedAudioAsset(const std::string& path)
    {
        std::string ext = std::filesystem::path(path).extension().string();
        std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
        return ext == ".wav" || ext == ".ogg" || ext == ".mp3" || ext == ".flac";
    }

    // AudioEmitter を持つ単体 entity 用 snapshot を作る。
    EntitySnapshot::Snapshot BuildAudioEmitterSnapshot(const std::string& name,
        const std::string& clipPath,
        bool playOnStart)
    {
        EntitySnapshot::Snapshot snapshot;
        snapshot.rootLocalID = 0;

        EntitySnapshot::Node node;
        node.localID = 0;
        node.sourceEntity = Entity::NULL_ID;
        node.parentLocalID = EntitySnapshot::kInvalidLocalID;
        node.externalParent = Entity::NULL_ID;

        TransformComponent transform{};
        transform.localScale = { 1.0f, 1.0f, 1.0f };
        transform.isDirty = true;

        AudioEmitterComponent emitter{};
        emitter.clipAssetPath = clipPath;
        emitter.playOnStart = playOnStart;
        emitter.is3D = true;
        emitter.bus = AudioBusType::SFX;

        // 既知クリップなら既定再生設定を流し込む。
        if (!clipPath.empty()) {
            const AudioClipAsset clip = EngineKernel::Instance().GetAudioWorld().DescribeClip(clipPath);
            emitter.streaming = clip.streaming;
            emitter.volume = clip.defaultVolume;
            emitter.pitch = clip.defaultPitch;
            emitter.loop = clip.defaultLoop;
        }

        std::get<std::optional<NameComponent>>(node.components) = NameComponent{ name };
        std::get<std::optional<TransformComponent>>(node.components) = transform;
        std::get<std::optional<HierarchyComponent>>(node.components) = HierarchyComponent{};
        std::get<std::optional<AudioEmitterComponent>>(node.components) = emitter;

        snapshot.nodes.push_back(std::move(node));
        return snapshot;
    }

    // Sprite 用 2D entity の snapshot を作る。
    EntitySnapshot::Snapshot BuildSingleSpriteSnapshot(const std::string& name,
        const std::string& texturePath)
    {
        EntitySnapshot::Snapshot snapshot;
        snapshot.rootLocalID = 0;

        EntitySnapshot::Node node;
        node.localID = 0;
        node.sourceEntity = Entity::NULL_ID;
        node.parentLocalID = EntitySnapshot::kInvalidLocalID;
        node.externalParent = Entity::NULL_ID;

        TransformComponent transform{};
        transform.localScale = { 1.0f, 1.0f, 1.0f };
        transform.isDirty = true;

        RectTransformComponent rect{};
        rect.sizeDelta = { 128.0f, 128.0f };

        CanvasItemComponent canvas{};
        SpriteComponent sprite{};
        sprite.textureAssetPath = texturePath;

        std::get<std::optional<NameComponent>>(node.components) = NameComponent{ name };
        std::get<std::optional<TransformComponent>>(node.components) = transform;
        std::get<std::optional<HierarchyComponent>>(node.components) = HierarchyComponent{};
        std::get<std::optional<RectTransformComponent>>(node.components) = rect;
        std::get<std::optional<CanvasItemComponent>>(node.components) = canvas;
        std::get<std::optional<SpriteComponent>>(node.components) = sprite;

        snapshot.nodes.push_back(std::move(node));
        return snapshot;
    }

    // Canvas 配下の空 2D entity 用 snapshot を作る。
    EntitySnapshot::Snapshot BuildSingleEmpty2DSnapshot(const std::string& name)
    {
        EntitySnapshot::Snapshot snapshot;
        snapshot.rootLocalID = 0;

        EntitySnapshot::Node node;
        node.localID = 0;
        node.sourceEntity = Entity::NULL_ID;
        node.parentLocalID = EntitySnapshot::kInvalidLocalID;
        node.externalParent = Entity::NULL_ID;

        TransformComponent transform{};
        transform.localScale = { 1.0f, 1.0f, 1.0f };
        transform.isDirty = true;

        RectTransformComponent rect{};
        rect.sizeDelta = { 128.0f, 128.0f };

        CanvasItemComponent canvas{};

        std::get<std::optional<NameComponent>>(node.components) = NameComponent{ name };
        std::get<std::optional<TransformComponent>>(node.components) = transform;
        std::get<std::optional<HierarchyComponent>>(node.components) = HierarchyComponent{};
        std::get<std::optional<RectTransformComponent>>(node.components) = rect;
        std::get<std::optional<CanvasItemComponent>>(node.components) = canvas;

        snapshot.nodes.push_back(std::move(node));
        return snapshot;
    }

    // テクスチャ実サイズが取れれば RectTransform の sizeDelta に反映する。
    void TryApplyTextureSize(EntitySnapshot::Snapshot& snapshot, const std::string& texturePath)
    {
        if (snapshot.nodes.empty()) {
            return;
        }

        auto texture = ResourceManager::Instance().GetTexture(texturePath);
        if (!texture) {
            return;
        }

        auto& rect = std::get<std::optional<RectTransformComponent>>(snapshot.nodes[0].components);
        if (rect.has_value()) {
            rect->sizeDelta = {
                static_cast<float>(texture->GetWidth()),
                static_cast<float>(texture->GetHeight())
            };
        }
    }

    // Text 用 2D entity の snapshot を作る。
    EntitySnapshot::Snapshot BuildSingleTextSnapshot(const std::string& name,
        const std::string& fontPath)
    {
        EntitySnapshot::Snapshot snapshot;
        snapshot.rootLocalID = 0;

        EntitySnapshot::Node node;
        node.localID = 0;
        node.sourceEntity = Entity::NULL_ID;
        node.parentLocalID = EntitySnapshot::kInvalidLocalID;
        node.externalParent = Entity::NULL_ID;

        TransformComponent transform{};
        transform.localScale = { 1.0f, 1.0f, 1.0f };
        transform.isDirty = true;

        RectTransformComponent rect{};
        rect.sizeDelta = { 320.0f, 80.0f };

        CanvasItemComponent canvas{};
        TextComponent text{};
        text.text = name;
        text.fontAssetPath = fontPath;

        std::get<std::optional<NameComponent>>(node.components) = NameComponent{ name };
        std::get<std::optional<TransformComponent>>(node.components) = transform;
        std::get<std::optional<HierarchyComponent>>(node.components) = HierarchyComponent{};
        std::get<std::optional<RectTransformComponent>>(node.components) = rect;
        std::get<std::optional<CanvasItemComponent>>(node.components) = canvas;
        std::get<std::optional<TextComponent>>(node.components) = text;

        snapshot.nodes.push_back(std::move(node));
        return snapshot;
    }

    // 既定 Sprite snapshot。
    EntitySnapshot::Snapshot BuildDefaultSpriteSnapshot()
    {
        return BuildSingleSpriteSnapshot("Sprite", "");
    }

    EntitySnapshot::Snapshot BuildDefaultUIButtonSnapshot()
    {
        EntitySnapshot::Snapshot snapshot = BuildSingleSpriteSnapshot("Button", "");
        if (!snapshot.nodes.empty()) {
            UIButtonComponent button{};
            button.buttonId = "Button";
            button.enabled = true;
            std::get<std::optional<UIButtonComponent>>(snapshot.nodes[0].components) = button;

            auto& rect = std::get<std::optional<RectTransformComponent>>(snapshot.nodes[0].components);
            if (rect.has_value()) {
                rect->sizeDelta = { 180.0f, 64.0f };
            }
        }
        return snapshot;
    }

    // 既定 Text snapshot。
    EntitySnapshot::Snapshot BuildDefaultTextSnapshot()
    {
        return BuildSingleTextSnapshot("Text", kDefault2DFontAssetPath);
    }

    // 2D Camera 用 snapshot を作る。
    EntitySnapshot::Snapshot BuildCamera2DSnapshot()
    {
        EntitySnapshot::Snapshot snapshot;
        snapshot.rootLocalID = 0;

        EntitySnapshot::Node node;
        node.localID = 0;
        node.sourceEntity = Entity::NULL_ID;
        node.parentLocalID = EntitySnapshot::kInvalidLocalID;
        node.externalParent = Entity::NULL_ID;

        TransformComponent transform{};
        transform.localPosition = { 0.0f, 0.0f, -100.0f };
        transform.localScale = { 1.0f, 1.0f, 1.0f };
        transform.isDirty = true;

        Camera2DComponent camera2D{};

        std::get<std::optional<NameComponent>>(node.components) = NameComponent{ "Camera 2D" };
        std::get<std::optional<TransformComponent>>(node.components) = transform;
        std::get<std::optional<HierarchyComponent>>(node.components) = HierarchyComponent{};
        std::get<std::optional<Camera2DComponent>>(node.components) = camera2D;

        snapshot.nodes.push_back(std::move(node));
        return snapshot;
    }

    // 可視/不可視切り替え対象の entity 群を返す。
    // 複数選択中なら選択ルートの MeshComponent 持ち全体を対象にする。
    std::vector<EntityID> GetVisibilityTargets(Registry& registry, EntityID entity)
    {
        auto& selection = EditorSelection::Instance();
        if (selection.IsEntitySelected(entity) && selection.GetSelectedEntityCount() > 1) {
            std::vector<EntityID> targets;
            for (EntityID selected : selection.GetSelectedEntities()) {
                if (registry.IsAlive(selected) && registry.GetComponent<MeshComponent>(selected)) {
                    targets.push_back(selected);
                }
            }
            if (!targets.empty()) {
                return targets;
            }
        }

        if (registry.GetComponent<MeshComponent>(entity)) {
            return { entity };
        }
        return {};
    }

    // Active/Inactive 切り替え対象の entity 群を返す。
    // 複数選択中なら選択ルートの HierarchyComponent 持ち全体を対象にする。
    std::vector<EntityID> GetActivationTargets(Registry& registry, EntityID entity)
    {
        auto& selection = EditorSelection::Instance();
        if (selection.IsEntitySelected(entity) && selection.GetSelectedEntityCount() > 1) {
            std::vector<EntityID> targets;
            for (EntityID selected : selection.GetSelectedEntities()) {
                if (registry.IsAlive(selected) && registry.GetComponent<HierarchyComponent>(selected)) {
                    targets.push_back(selected);
                }
            }
            if (!targets.empty()) {
                return targets;
            }
        }

        if (registry.GetComponent<HierarchyComponent>(entity)) {
            return { entity };
        }
        return {};
    }

    // 対象群の MeshComponent::isVisible を変更し、Undo を積む。
    void SetVisibilityForTargets(Registry& registry, const std::vector<EntityID>& targets, bool visible)
    {
        auto composite = std::make_unique<CompositeUndoAction>(visible ? "Show Entities" : "Hide Entities");

        for (EntityID target : targets) {
            auto* mesh = registry.GetComponent<MeshComponent>(target);
            if (!mesh || mesh->isVisible == visible) {
                continue;
            }

            MeshComponent before = *mesh;
            mesh->isVisible = visible;
            MeshComponent after = *mesh;
            composite->Add(std::make_unique<ComponentUndoAction<MeshComponent>>(target, before, after));
        }

        if (!composite->Empty()) {
            UndoSystem::Instance().RecordAction(std::move(composite));
        }
    }

    // 対象群の HierarchyComponent::isActive を変更し、Undo を積む。
    void SetActiveForTargets(Registry& registry, const std::vector<EntityID>& targets, bool active)
    {
        auto composite = std::make_unique<CompositeUndoAction>(active ? "Activate Entities" : "Deactivate Entities");

        for (EntityID target : targets) {
            auto* hierarchy = registry.GetComponent<HierarchyComponent>(target);
            if (!hierarchy || hierarchy->isActive == active) {
                continue;
            }

            HierarchyComponent before = *hierarchy;
            hierarchy->isActive = active;
            HierarchyComponent after = *hierarchy;
            composite->Add(std::make_unique<ComponentUndoAction<HierarchyComponent>>(target, before, after));
        }

        if (!composite->Empty()) {
            UndoSystem::Instance().RecordAction(std::move(composite));
        }
    }

    // 方向ライト用 snapshot を作る。
    EntitySnapshot::Snapshot BuildDirectionalLightSnapshot()
    {
        LightComponent directionalLight;
        directionalLight.type = LightType::Directional;
        directionalLight.castShadow = true;

        auto snapshot = BuildSingleEntitySnapshot("Directional Light", nullptr, &directionalLight, nullptr);
        auto& transform = std::get<std::optional<TransformComponent>>(snapshot.nodes[0].components);
        if (transform.has_value()) {
            using namespace DirectX;
            XMVECTOR rot = XMQuaternionRotationRollPitchYaw(XMConvertToRadians(45.0f), XMConvertToRadians(45.0f), 0.0f);
            XMStoreFloat4(&transform->localRotation, rot);
        }
        return snapshot;
    }

    // snapshot から entity を生成し、Undo を積み、生成物を選択する。
    bool CreateEntityFromSnapshot(Registry* registry,
        EntitySnapshot::Snapshot snapshot,
        EntityID parentEntity,
        const char* actionName)
    {
        if (!registry || snapshot.nodes.empty()) {
            return false;
        }

        auto action = std::make_unique<CreateEntityAction>(std::move(snapshot), parentEntity, actionName);
        auto* actionPtr = action.get();
        UndoSystem::Instance().ExecuteAction(std::move(action), *registry);
        EditorSelection::Instance().SelectEntity(actionPtr->GetLiveRoot());
        return true;
    }
}

// 次フレームで検索ボックスへフォーカスする。
void HierarchyECSUI::RequestSearchFocus()
{
    s_requestHierarchySearchFocus = true;
}

// Hierarchy ウィンドウ全体を描画する。
void HierarchyECSUI::Render(Registry* registry, bool* p_open, bool* outFocused) {
    if (!ImGui::Begin(ICON_FA_LIST " Hierarchy", p_open)) {
        if (outFocused) {
            *outFocused = false;
        }
        ImGui::End();
        return;
    }

    if (outFocused) {
        *outFocused = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);
    }

    if (!registry) {
        ImGui::TextDisabled("No Active Scene");
        ImGui::End();
        return;
    }

    // ------------------------------------------
    // 検索ボックス
    // ------------------------------------------
    static std::array<char, 256> searchBuffer{};
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(6.0f, 4.0f));
    ImGui::SetNextItemWidth(-1.0f);

    if (s_requestHierarchySearchFocus) {
        ImGui::SetKeyboardFocusHere();
        s_requestHierarchySearchFocus = false;
    }

    if (ImGui::InputTextWithHint("##HierarchySearch", ICON_FA_MAGNIFYING_GLASS " Search hierarchy", searchBuffer.data(), searchBuffer.size())) {
        s_hierarchyFilterLower = ToLowerCopy(searchBuffer.data());
    }
    ImGui::PopStyleVar();

    if (!s_hierarchyFilterLower.empty()) {
        ImGui::SameLine();
        if (ImGui::SmallButton("Clear")) {
            searchBuffer[0] = '\0';
            s_hierarchyFilterLower.clear();
        }
    }
    ImGui::Spacing();

    // ------------------------------------------
    // ルート entity を走査してツリー描画
    // ------------------------------------------
    auto archetypes = registry->GetAllArchetypes();
    for (auto* archetype : archetypes) {
        const auto& entities = archetype->GetEntities();
        for (EntityID entity : entities) {
            HierarchyComponent* hier = registry->GetComponent<HierarchyComponent>(entity);
            if (!hier || Entity::IsNull(hier->parent)) {
                if (registry->GetComponent<EnvironmentComponent>(entity) ||
                    registry->GetComponent<ReflectionProbeComponent>(entity) ||
                    registry->GetComponent<AudioSettingsComponent>(entity) ||
                    registry->GetComponent<SequencerPreviewCameraComponent>(entity)) {
                    continue;
                }
                if (!SubtreeMatchesFilter(*registry, entity)) {
                    continue;
                }
                DrawEntityNode(registry, entity);
            }
        }
    }

    // ------------------------------------------
    // 何もない領域への D&D / 右クリックメニュー
    // ------------------------------------------
    ImVec2 availSize = ImGui::GetContentRegionAvail();
    if (availSize.x <= 0.0f) { availSize.x = 1.0f; }
    if (availSize.y < 50.0f) { availSize.y = 50.0f; }

    ImGui::Dummy(availSize);
    HandleDragDropTarget(registry, Entity::NULL_ID);

    if (ImGui::BeginPopupContextWindow("HierarchyContextMenu", ImGuiPopupFlags_MouseButtonRight | ImGuiPopupFlags_NoOpenOverItems)) {
        if (ImGui::MenuItem("Create Empty Entity")) {
            CreateEntityFromSnapshot(registry, BuildSingleEntitySnapshot("Empty Entity"), Entity::NULL_ID, "Create Entity");
        }
        if (ImGui::MenuItem("Create Empty (2D)")) {
            CreateEntityFromSnapshot(registry, BuildSingleEmpty2DSnapshot("Empty 2D"), Entity::NULL_ID, "Create Empty 2D");
        }
        if (ImGui::MenuItem("Create Sprite")) {
            CreateEntityFromSnapshot(registry, BuildDefaultSpriteSnapshot(), Entity::NULL_ID, "Create Sprite");
        }
        if (ImGui::MenuItem("Create UI Button")) {
            CreateEntityFromSnapshot(registry, BuildDefaultUIButtonSnapshot(), Entity::NULL_ID, "Create UI Button");
        }
        if (ImGui::MenuItem("Create Text")) {
            CreateEntityFromSnapshot(registry, BuildDefaultTextSnapshot(), Entity::NULL_ID, "Create Text");
        }

        LightComponent pointLight;
        pointLight.type = LightType::Point;
        pointLight.range = 10.0f;
        pointLight.castShadow = false;
        if (ImGui::MenuItem("Create Point Light")) {
            CreateEntityFromSnapshot(registry,
                BuildSingleEntitySnapshot("Point Light", nullptr, &pointLight, nullptr),
                Entity::NULL_ID,
                "Create Point Light");
        }

        if (ImGui::MenuItem("Create Directional Light")) {
            CreateEntityFromSnapshot(registry, BuildDirectionalLightSnapshot(), Entity::NULL_ID, "Create Directional Light");
        }

        ReflectionProbeComponent probe;
        probe.needsBake = true;
        if (ImGui::MenuItem("Create Reflection Probe")) {
            CreateEntityFromSnapshot(registry,
                BuildSingleEntitySnapshot("Reflection Probe", nullptr, nullptr, &probe),
                Entity::NULL_ID,
                "Create Reflection Probe");
        }

        // 選択アセットに応じた生成メニュー。
        const auto& selection = EditorSelection::Instance();
        if (selection.GetType() == SelectionType::Asset && IsSupportedModelAsset(selection.GetAssetPath())) {
            if (ImGui::MenuItem("Create From Selected Model")) {
                MeshComponent meshComp;
                meshComp.modelFilePath = selection.GetAssetPath();
                meshComp.model = ResourceManager::Instance().CreateModelInstance(selection.GetAssetPath());
                const std::string name = std::filesystem::path(meshComp.modelFilePath).stem().string();
                CreateEntityFromSnapshot(registry,
                    BuildSingleEntitySnapshot(name, &meshComp, nullptr, nullptr),
                    Entity::NULL_ID,
                    "Create Entity From Model");
            }
        }

        if (selection.GetType() == SelectionType::Asset && IsSupportedSpriteAsset(selection.GetAssetPath())) {
            if (ImGui::MenuItem("Create Sprite From Selected Texture")) {
                const std::string name = std::filesystem::path(selection.GetAssetPath()).stem().string();
                auto snapshot = BuildSingleSpriteSnapshot(name, selection.GetAssetPath());
                TryApplyTextureSize(snapshot, selection.GetAssetPath());
                CreateEntityFromSnapshot(registry, std::move(snapshot), Entity::NULL_ID, "Create Sprite");
            }
        }

        if (selection.GetType() == SelectionType::Asset && IsSupportedFontAsset(selection.GetAssetPath())) {
            if (ImGui::MenuItem("Create Text From Selected Font")) {
                const std::string name = std::filesystem::path(selection.GetAssetPath()).stem().string();
                CreateEntityFromSnapshot(registry,
                    BuildSingleTextSnapshot(name, selection.GetAssetPath()),
                    Entity::NULL_ID,
                    "Create Text");
            }
        }

        if (selection.GetType() == SelectionType::Asset && IsSupportedAudioAsset(selection.GetAssetPath())) {
            if (ImGui::MenuItem("Create Audio Source From Selected Clip")) {
                const std::string name = std::filesystem::path(selection.GetAssetPath()).stem().string();
                CreateEntityFromSnapshot(registry,
                    BuildAudioEmitterSnapshot(name, selection.GetAssetPath(), true),
                    Entity::NULL_ID,
                    "Create Audio Source");
            }
        }

        if (ImGui::MenuItem("Create 2D Camera")) {
            CreateEntityFromSnapshot(registry, BuildCamera2DSnapshot(), Entity::NULL_ID, "Create 2D Camera");
        }
        if (ImGui::MenuItem("Create Audio Source")) {
            CreateEntityFromSnapshot(registry, BuildAudioEmitterSnapshot("Audio Source", "", false), Entity::NULL_ID, "Create Audio Source");
        }

        ImGui::Separator();

        // Input 系テンプレート作成。
        if (ImGui::BeginMenu("Input")) {
            if (ImGui::MenuItem("Input User")) {
                EntityID e = registry->CreateEntity();
                registry->AddComponent(e, NameComponent{ "Input User" });
                registry->AddComponent(e, TransformComponent{});
                registry->AddComponent(e, HierarchyComponent{});
                InputUserComponent user{};
                user.userId = 1;
                user.isPrimary = true;
                registry->AddComponent(e, user);
                registry->AddComponent(e, InputBindingComponent{});
                registry->AddComponent(e, InputContextComponent{});
                registry->AddComponent(e, ResolvedInputStateComponent{});
            }

            if (ImGui::MenuItem("Input Listener")) {
                EntityID e = registry->CreateEntity();
                registry->AddComponent(e, NameComponent{ "Input Listener" });
                registry->AddComponent(e, TransformComponent{});
                registry->AddComponent(e, HierarchyComponent{});
                registry->AddComponent(e, InputContextComponent{});
                registry->AddComponent(e, ResolvedInputStateComponent{});
            }

            if (ImGui::MenuItem("Text Input Target")) {
                EntityID e = registry->CreateEntity();
                registry->AddComponent(e, NameComponent{ "Text Input Target" });
                registry->AddComponent(e, TransformComponent{});
                registry->AddComponent(e, HierarchyComponent{});
                InputContextComponent ctx{};
                ctx.priority = InputContextPriority::TextInput;
                ctx.textInputEnabled = true;
                ctx.consumeLowerPriority = true;
                registry->AddComponent(e, ctx);
                registry->AddComponent(e, InputTextFieldComponent{});
            }

            if (ImGui::MenuItem("Vibration Source")) {
                EntityID e = registry->CreateEntity();
                registry->AddComponent(e, NameComponent{ "Vibration Source" });
                registry->AddComponent(e, TransformComponent{});
                registry->AddComponent(e, HierarchyComponent{});
                registry->AddComponent(e, VibrationRequestComponent{});
            }

            ImGui::EndMenu();
        }

        // Gameplay 系テンプレート作成。
        if (ImGui::BeginMenu("Gameplay")) {
            if (ImGui::MenuItem("Player")) {
                EntityID e = registry->CreateEntity();
                registry->AddComponent(e, NameComponent{ "Player" });
                registry->AddComponent(e, TransformComponent{});
                registry->AddComponent(e, HierarchyComponent{});
                PlayerRuntimeSetup::EnsurePlayerPersistentComponents(*registry, e);
                PlayerRuntimeSetup::EnsurePlayerRuntimeComponents(*registry, e);
                PlayerRuntimeSetup::ResetPlayerRuntimeState(*registry, e);
            }

            if (ImGui::MenuItem("Character")) {
                EntityID e = registry->CreateEntity();
                registry->AddComponent(e, NameComponent{ "Character" });
                registry->AddComponent(e, TransformComponent{});
                registry->AddComponent(e, HierarchyComponent{});
                registry->AddComponent(e, CharacterPhysicsComponent{});
                registry->AddComponent(e, HealthComponent{});
                registry->AddComponent(e, LocomotionStateComponent{});
            }

            if (ImGui::MenuItem("Timeline Entity")) {
                EntityID e = registry->CreateEntity();
                registry->AddComponent(e, NameComponent{ "Timeline Entity" });
                registry->AddComponent(e, TransformComponent{});
                registry->AddComponent(e, HierarchyComponent{});
                registry->AddComponent(e, PlaybackComponent{});
                registry->AddComponent(e, TimelineComponent{});
                registry->AddComponent(e, TimelineItemBuffer{});
            }

            ImGui::EndMenu();
        }

        ImGui::EndPopup();
    }

    ImGui::End();
}

// 1 entity 分のノードを再帰描画する。
void HierarchyECSUI::DrawEntityNode(Registry* registry, EntityID entity) {
    if (!registry ||
        registry->GetComponent<EnvironmentComponent>(entity) ||
        registry->GetComponent<ReflectionProbeComponent>(entity) ||
        registry->GetComponent<AudioSettingsComponent>(entity) ||
        registry->GetComponent<SequencerPreviewCameraComponent>(entity) ||
        !SubtreeMatchesFilter(*registry, entity)) {
        return;
    }

    auto* nameComp = registry->GetComponent<NameComponent>(entity);
    auto* hierComp = registry->GetComponent<HierarchyComponent>(entity);

    std::string entityName = nameComp ? nameComp->name : "Entity_" + std::to_string(Entity::GetIndex(entity));
    std::string idStr = "##" + std::to_string(Entity::GetIndex(entity));

    bool hasChildren = (hierComp && !Entity::IsNull(hierComp->firstChild));

    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth;
    if (!hasChildren) {
        flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
    }
    if (!s_hierarchyFilterLower.empty()) {
        flags |= ImGuiTreeNodeFlags_DefaultOpen;
    }

    if (EditorSelection::Instance().GetType() == SelectionType::Entity &&
        EditorSelection::Instance().IsEntitySelected(entity)) {
        flags |= ImGuiTreeNodeFlags_Selected;
    }

    ImGui::PushID(static_cast<int>(Entity::GetIndex(entity)));

    // 非アクティブ entity は文字色を落として描く。
    if (hierComp && !hierComp->isActive) {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.55f, 0.55f, 0.55f, 1.0f));
    }

    bool isOpen = ImGui::TreeNodeEx((entityName + idStr).c_str(), flags, "%s %s", ICON_FA_CUBE, entityName.c_str());

    if (hierComp && !hierComp->isActive) {
        ImGui::PopStyleColor();
    }

    const ImVec2 itemRectMin = ImGui::GetItemRectMin();

    // 左クリックで選択。
    if (ImGui::IsItemClicked(ImGuiMouseButton_Left)) {
        HandleEntitySelectionClick(entity);
    }

    // 右端の active / visible ボタン群を描く。
    float buttonX = ImGui::GetWindowPos().x + ImGui::GetWindowContentRegionMax().x - ImGui::GetFrameHeight() - 4.0f;

    if (hierComp) {
        const char* activeIcon = hierComp->isActive ? ICON_FA_POWER_OFF : ICON_FA_BOLT;
        ImGui::SetCursorScreenPos(ImVec2(buttonX, itemRectMin.y));
        if (ImGui::SmallButton(activeIcon)) {
            SetActiveForTargets(*registry, GetActivationTargets(*registry, entity), !hierComp->isActive);
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip(hierComp->isActive ? "Deactivate" : "Activate");
        }
        buttonX -= ImGui::GetFrameHeight() + 4.0f;
    }

    if (auto* mesh = registry->GetComponent<MeshComponent>(entity)) {
        const float buttonSize = ImGui::GetFrameHeight();
        (void)buttonSize;
        ImGui::SetCursorScreenPos(ImVec2(buttonX, itemRectMin.y));
        const char* icon = mesh->isVisible ? ICON_FA_EYE : ICON_FA_EYE_SLASH;
        if (ImGui::SmallButton(icon)) {
            SetVisibilityForTargets(*registry, { entity }, !mesh->isVisible);
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip(mesh->isVisible ? "Hide" : "Show");
        }
    }

    // entity D&D source。
    if (ImGui::BeginDragDropSource()) {
        EntityID payloadEntity = entity;
        ImGui::SetDragDropPayload("ENGINE_ENTITY", &payloadEntity, sizeof(payloadEntity));
        ImGui::Text("%s %s", ICON_FA_CUBE, entityName.c_str());
        ImGui::EndDragDropSource();
    }

    // ノード右クリックメニュー。
    const std::string popupId = "HierarchyEntityContext##" + std::to_string(static_cast<unsigned long long>(entity));
    if (ImGui::BeginPopupContextItem(popupId.c_str())) {
        const std::vector<EntityID> activationTargets = GetActivationTargets(*registry, entity);
        if (!activationTargets.empty()) {
            bool anyActive = false;
            bool anyInactive = false;
            for (EntityID target : activationTargets) {
                if (auto* targetHierarchy = registry->GetComponent<HierarchyComponent>(target)) {
                    anyActive |= targetHierarchy->isActive;
                    anyInactive |= !targetHierarchy->isActive;
                }
            }

            if (anyActive && ImGui::MenuItem(ICON_FA_POWER_OFF " Deactivate")) {
                SetActiveForTargets(*registry, activationTargets, false);
            }
            if (anyInactive && ImGui::MenuItem(ICON_FA_BOLT " Activate")) {
                SetActiveForTargets(*registry, activationTargets, true);
            }
            ImGui::Separator();
        }

        const std::vector<EntityID> visibilityTargets = GetVisibilityTargets(*registry, entity);
        if (!visibilityTargets.empty()) {
            bool anyVisible = false;
            bool anyHidden = false;
            for (EntityID target : visibilityTargets) {
                if (auto* mesh = registry->GetComponent<MeshComponent>(target)) {
                    anyVisible |= mesh->isVisible;
                    anyHidden |= !mesh->isVisible;
                }
            }

            if (anyVisible && ImGui::MenuItem(ICON_FA_EYE_SLASH " Hide")) {
                SetVisibilityForTargets(*registry, visibilityTargets, false);
            }
            if (anyHidden && ImGui::MenuItem(ICON_FA_EYE " Show")) {
                SetVisibilityForTargets(*registry, visibilityTargets, true);
            }
            ImGui::Separator();
        }

        // 複製。
        if (ImGui::MenuItem("Duplicate")) {
            auto& selection = EditorSelection::Instance();
            if (!selection.IsEntitySelected(entity)) {
                selection.SelectEntity(entity);
            }

            const std::vector<EntityID> selectedRoots = GetSelectedRootEntities(*registry, selection);
            auto composite = std::make_unique<CompositeUndoAction>("Duplicate Entities");
            std::vector<DuplicateEntityAction*> duplicateActions;

            for (EntityID selectedRoot : selectedRoots) {
                if (!PrefabSystem::CanDuplicate(selectedRoot, *registry)) {
                    LOG_WARN("[Prefab] Prefab instance children cannot be duplicated. Duplicate the root instance or unpack it first.");
                    continue;
                }

                EntitySnapshot::Snapshot snapshot = EntitySnapshot::CaptureSubtree(selectedRoot, *registry);
                EntityID parentEntity = Entity::NULL_ID;
                if (auto* hierarchy = registry->GetComponent<HierarchyComponent>(selectedRoot)) {
                    parentEntity = hierarchy->parent;
                }

                EntitySnapshot::AppendRootNameSuffix(snapshot, " (Clone)");
                auto action = std::make_unique<DuplicateEntityAction>(std::move(snapshot), parentEntity);
                auto* actionPtr = action.get();
                composite->Add(std::move(action));
                duplicateActions.push_back(actionPtr);
            }

            if (!composite->Empty()) {
                UndoSystem::Instance().ExecuteAction(std::move(composite), *registry);
                std::vector<EntityID> liveRoots;
                for (DuplicateEntityAction* action : duplicateActions) {
                    if (!Entity::IsNull(action->GetLiveRoot())) {
                        liveRoots.push_back(action->GetLiveRoot());
                    }
                }
                if (!liveRoots.empty()) {
                    EditorSelection::Instance().SetEntitySelection(liveRoots, liveRoots.back());
                }
            }
        }

        // 削除。
        if (ImGui::MenuItem("Delete")) {
            auto& selection = EditorSelection::Instance();
            if (!selection.IsEntitySelected(entity)) {
                selection.SelectEntity(entity);
            }

            const std::vector<EntityID> selectedRoots = GetSelectedRootEntities(*registry, selection);
            auto composite = std::make_unique<CompositeUndoAction>("Delete Entities");
            bool canDeleteAny = false;

            for (EntityID selectedRoot : selectedRoots) {
                if (!PrefabSystem::CanDelete(selectedRoot, *registry)) {
                    LOG_WARN("[Prefab] Prefab instance children cannot be deleted directly. Use Unpack first.");
                    continue;
                }

                EntitySnapshot::Snapshot snapshot = EntitySnapshot::CaptureSubtree(selectedRoot, *registry);
                composite->Add(std::make_unique<DeleteEntityAction>(std::move(snapshot), selectedRoot));
                canDeleteAny = true;
            }

            if (canDeleteAny) {
                UndoSystem::Instance().ExecuteAction(std::move(composite), *registry);
                EditorSelection::Instance().Clear();
            }
        }

        // 子作成系メニュー。
        if (ImGui::MenuItem("Create Empty Child")) {
            if (!PrefabSystem::CanCreateChild(entity, *registry)) {
                LOG_WARN("[Prefab] Prefab instance hierarchy is locked. Use Unpack before adding children.");
            }
            else {
                CreateEntityFromSnapshot(registry, BuildSingleEntitySnapshot("Empty Entity"), entity, "Create Child Entity");
            }
        }

        if (ImGui::MenuItem("Create Empty Child (2D)")) {
            if (!PrefabSystem::CanCreateChild(entity, *registry)) {
                LOG_WARN("[Prefab] Prefab instance hierarchy is locked. Use Unpack before adding children.");
            }
            else {
                CreateEntityFromSnapshot(registry, BuildSingleEmpty2DSnapshot("Empty 2D"), entity, "Create 2D Child");
            }
        }

        if (ImGui::MenuItem("Create Sprite Child")) {
            if (!PrefabSystem::CanCreateChild(entity, *registry)) {
                LOG_WARN("[Prefab] Prefab instance hierarchy is locked. Use Unpack before adding children.");
            }
            else {
                CreateEntityFromSnapshot(registry, BuildDefaultSpriteSnapshot(), entity, "Create Sprite Child");
            }
        }

        if (ImGui::MenuItem("Create UI Button Child")) {
            if (!PrefabSystem::CanCreateChild(entity, *registry)) {
                LOG_WARN("[Prefab] Prefab instance hierarchy is locked. Use Unpack before adding children.");
            }
            else {
                CreateEntityFromSnapshot(registry, BuildDefaultUIButtonSnapshot(), entity, "Create UI Button Child");
            }
        }

        if (ImGui::MenuItem("Create Text Child")) {
            if (!PrefabSystem::CanCreateChild(entity, *registry)) {
                LOG_WARN("[Prefab] Prefab instance hierarchy is locked. Use Unpack before adding children.");
            }
            else {
                CreateEntityFromSnapshot(registry, BuildDefaultTextSnapshot(), entity, "Create Text Child");
            }
        }

        LightComponent pointLight;
        pointLight.type = LightType::Point;
        pointLight.range = 10.0f;
        pointLight.castShadow = false;
        if (ImGui::MenuItem("Create Point Light Child")) {
            if (!PrefabSystem::CanCreateChild(entity, *registry)) {
                LOG_WARN("[Prefab] Prefab instance hierarchy is locked. Use Unpack before adding children.");
            }
            else {
                CreateEntityFromSnapshot(registry,
                    BuildSingleEntitySnapshot("Point Light", nullptr, &pointLight, nullptr),
                    entity,
                    "Create Point Light Child");
            }
        }

        if (ImGui::MenuItem("Create Directional Light Child")) {
            if (!PrefabSystem::CanCreateChild(entity, *registry)) {
                LOG_WARN("[Prefab] Prefab instance hierarchy is locked. Use Unpack before adding children.");
            }
            else {
                CreateEntityFromSnapshot(registry, BuildDirectionalLightSnapshot(), entity, "Create Directional Light Child");
            }
        }

        ReflectionProbeComponent probe;
        probe.needsBake = true;
        if (ImGui::MenuItem("Create Reflection Probe Child")) {
            if (!PrefabSystem::CanCreateChild(entity, *registry)) {
                LOG_WARN("[Prefab] Prefab instance hierarchy is locked. Use Unpack before adding children.");
            }
            else {
                CreateEntityFromSnapshot(registry,
                    BuildSingleEntitySnapshot("Reflection Probe", nullptr, nullptr, &probe),
                    entity,
                    "Create Reflection Probe Child");
            }
        }

        const auto& selection = EditorSelection::Instance();
        if (selection.GetType() == SelectionType::Asset && IsSupportedModelAsset(selection.GetAssetPath())) {
            if (ImGui::MenuItem("Create Model Child")) {
                if (!PrefabSystem::CanCreateChild(entity, *registry)) {
                    LOG_WARN("[Prefab] Prefab instance hierarchy is locked. Use Unpack before adding children.");
                }
                else {
                    MeshComponent meshComp;
                    meshComp.modelFilePath = selection.GetAssetPath();
                    meshComp.model = ResourceManager::Instance().CreateModelInstance(selection.GetAssetPath());
                    const std::string name = std::filesystem::path(meshComp.modelFilePath).stem().string();
                    CreateEntityFromSnapshot(registry,
                        BuildSingleEntitySnapshot(name, &meshComp, nullptr, nullptr),
                        entity,
                        "Create Model Child");
                }
            }
        }

        if (selection.GetType() == SelectionType::Asset && IsSupportedSpriteAsset(selection.GetAssetPath())) {
            if (ImGui::MenuItem("Create Sprite Child From Selected Texture")) {
                if (!PrefabSystem::CanCreateChild(entity, *registry)) {
                    LOG_WARN("[Prefab] Prefab instance hierarchy is locked. Use Unpack before adding children.");
                }
                else {
                    const std::string name = std::filesystem::path(selection.GetAssetPath()).stem().string();
                    auto snapshot = BuildSingleSpriteSnapshot(name, selection.GetAssetPath());
                    TryApplyTextureSize(snapshot, selection.GetAssetPath());
                    CreateEntityFromSnapshot(registry, std::move(snapshot), entity, "Create Sprite Child");
                }
            }
        }

        if (selection.GetType() == SelectionType::Asset && IsSupportedFontAsset(selection.GetAssetPath())) {
            if (ImGui::MenuItem("Create Text Child From Selected Font")) {
                if (!PrefabSystem::CanCreateChild(entity, *registry)) {
                    LOG_WARN("[Prefab] Prefab instance hierarchy is locked. Use Unpack before adding children.");
                }
                else {
                    const std::string name = std::filesystem::path(selection.GetAssetPath()).stem().string();
                    CreateEntityFromSnapshot(registry,
                        BuildSingleTextSnapshot(name, selection.GetAssetPath()),
                        entity,
                        "Create Text Child");
                }
            }
        }

        if (selection.GetType() == SelectionType::Asset && IsSupportedAudioAsset(selection.GetAssetPath())) {
            if (ImGui::MenuItem("Create Audio Source Child From Selected Clip")) {
                if (!PrefabSystem::CanCreateChild(entity, *registry)) {
                    LOG_WARN("[Prefab] Prefab instance hierarchy is locked. Use Unpack before adding children.");
                }
                else {
                    const std::string name = std::filesystem::path(selection.GetAssetPath()).stem().string();
                    CreateEntityFromSnapshot(registry,
                        BuildAudioEmitterSnapshot(name, selection.GetAssetPath(), true),
                        entity,
                        "Create Audio Source Child");
                }
            }
        }

        if (ImGui::MenuItem("Create 2D Camera Child")) {
            if (!PrefabSystem::CanCreateChild(entity, *registry)) {
                LOG_WARN("[Prefab] Prefab instance hierarchy is locked. Use Unpack before adding children.");
            }
            else {
                CreateEntityFromSnapshot(registry, BuildCamera2DSnapshot(), entity, "Create 2D Camera Child");
            }
        }

        if (ImGui::MenuItem("Create Audio Source Child")) {
            if (!PrefabSystem::CanCreateChild(entity, *registry)) {
                LOG_WARN("[Prefab] Prefab instance hierarchy is locked. Use Unpack before adding children.");
            }
            else {
                CreateEntityFromSnapshot(registry, BuildAudioEmitterSnapshot("Audio Source", "", false), entity, "Create Audio Source Child");
            }
        }

        ImGui::EndPopup();
    }

    // このノードへ D&D できるようにする。
    HandleDragDropTarget(registry, entity);

    // 子ノードを再帰描画する。
    if (isOpen && hasChildren) {
        EntityID currentChild = hierComp->firstChild;
        while (!Entity::IsNull(currentChild)) {
            DrawEntityNode(registry, currentChild);

            auto* childHier = registry->GetComponent<HierarchyComponent>(currentChild);
            currentChild = childHier ? childHier->nextSibling : Entity::NULL_ID;
        }
        ImGui::TreePop();
    }

    ImGui::PopID();
}

// entity や asset の D&D を受け付けて、Reparent や生成を行う。
void HierarchyECSUI::HandleDragDropTarget(Registry* registry, EntityID parentEntity) {
    if (ImGui::BeginDragDropTarget()) {
        // ------------------------------------------
        // Entity の D&D: Reparent
        // ------------------------------------------
        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ENGINE_ENTITY")) {
            if (payload->DataSize == sizeof(EntityID)) {
                EntityID draggedEntity = *static_cast<const EntityID*>(payload->Data);
                if (registry->IsAlive(draggedEntity) &&
                    draggedEntity != parentEntity &&
                    !HierarchySystem::WouldCreateCycle(draggedEntity, parentEntity, *registry)) {

                    if (!PrefabSystem::CanReparent(draggedEntity, parentEntity, *registry)) {
                        LOG_WARN("[Prefab] Prefab instance internal hierarchy cannot be reparented. Use Unpack first.");
                        ImGui::EndDragDropTarget();
                        return;
                    }

                    EntityID oldParent = Entity::NULL_ID;
                    if (auto* hierarchy = registry->GetComponent<HierarchyComponent>(draggedEntity)) {
                        oldParent = hierarchy->parent;
                    }

                    UndoSystem::Instance().ExecuteAction(
                        std::make_unique<ReparentEntityAction>(draggedEntity, parentEntity, oldParent, true),
                        *registry);

                    EditorSelection::Instance().SelectEntity(draggedEntity);
                }
            }
        }

        // ------------------------------------------
        // Asset の D&D: 生成 / 割り当て
        // ------------------------------------------
        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ENGINE_ASSET")) {
            std::string sourcePathStr((const char*)payload->Data);
            std::filesystem::path path(sourcePathStr);
            std::string ext = path.extension().string();
            std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

            // Material は既存 entity へ割り当てる。
            if (ext == ".mat") {
                if (!Entity::IsNull(parentEntity)) {
                    auto* matComp = registry->GetComponent<MaterialComponent>(parentEntity);
                    MaterialComponent before = matComp ? *matComp : MaterialComponent{};
                    const bool hadComponent = (matComp != nullptr);

                    if (!matComp) {
                        MaterialComponent newMatComp;
                        newMatComp.materialAssetPath = sourcePathStr;
                        newMatComp.materialAsset = ResourceManager::Instance().GetMaterial(sourcePathStr);
                        registry->AddComponent(parentEntity, newMatComp);
                    }
                    else {
                        matComp->materialAssetPath = sourcePathStr;
                        matComp->materialAsset = ResourceManager::Instance().GetMaterial(sourcePathStr);
                    }

                    MaterialComponent after = *registry->GetComponent<MaterialComponent>(parentEntity);
                    UndoSystem::Instance().RecordAction(
                        std::make_unique<OptionalComponentUndoAction<MaterialComponent>>(
                            parentEntity,
                            hadComponent ? std::optional<MaterialComponent>(before) : std::nullopt,
                            std::optional<MaterialComponent>(after),
                            "Assign Material"));

                    PrefabSystem::MarkPrefabOverride(parentEntity, *registry);
                    EditorSelection::Instance().SelectEntity(parentEntity);
                }
            }
            // Prefab は Instantiate する。
            else if (ext == ".prefab") {
                if (!PrefabSystem::CanCreateChild(parentEntity, *registry)) {
                    LOG_WARN("[Prefab] Prefab instance hierarchy is locked. Use Unpack before adding children.");
                    ImGui::EndDragDropTarget();
                    return;
                }

                EntityID newEntity = PrefabSystem::InstantiatePrefab(sourcePathStr, *registry, parentEntity);
                if (!Entity::IsNull(newEntity)) {
                    EntitySnapshot::Snapshot snapshot = EntitySnapshot::CaptureSubtree(newEntity, *registry);
                    auto action = std::make_unique<CreateEntityAction>(std::move(snapshot), parentEntity, "Instantiate Prefab");
                    action->AdoptLiveRoot(newEntity);
                    UndoSystem::Instance().RecordAction(std::move(action));
                    EditorSelection::Instance().SelectEntity(newEntity);
                }
            }
            // モデルは MeshComponent 付き entity を生成する。
            else if (ext == ".fbx" || ext == ".obj" || ext == ".blend" || ext == ".gltf") {
                MeshComponent meshComp;
                meshComp.modelFilePath = sourcePathStr;
                meshComp.model = ResourceManager::Instance().CreateModelInstance(sourcePathStr);

                if (!PrefabSystem::CanCreateChild(parentEntity, *registry)) {
                    LOG_WARN("[Prefab] Prefab instance hierarchy is locked. Use Unpack before adding children.");
                    ImGui::EndDragDropTarget();
                    return;
                }

                auto action = std::make_unique<CreateEntityAction>(
                    BuildSingleEntitySnapshot(path.stem().string(), &meshComp),
                    parentEntity,
                    "Create Entity From Asset");
                auto* actionPtr = action.get();
                UndoSystem::Instance().ExecuteAction(std::move(action), *registry);
                EditorSelection::Instance().SelectEntity(actionPtr->GetLiveRoot());
            }
            // Sprite texture は Sprite entity を生成する。
            else if (IsSupportedSpriteAsset(sourcePathStr)) {
                if (!PrefabSystem::CanCreateChild(parentEntity, *registry)) {
                    LOG_WARN("[Prefab] Prefab instance hierarchy is locked. Use Unpack before adding children.");
                    ImGui::EndDragDropTarget();
                    return;
                }

                auto action = std::make_unique<CreateEntityAction>(
                    [&, sourcePathStr]() {
                        auto snapshot = BuildSingleSpriteSnapshot(path.stem().string(), sourcePathStr);
                        TryApplyTextureSize(snapshot, sourcePathStr);
                        return snapshot;
                    }(),
                        parentEntity,
                        "Create Sprite From Asset");
                auto* actionPtr = action.get();
                UndoSystem::Instance().ExecuteAction(std::move(action), *registry);
                EditorSelection::Instance().SelectEntity(actionPtr->GetLiveRoot());
            }
            // Font asset は Text entity を生成する。
            else if (IsSupportedFontAsset(sourcePathStr)) {
                if (!PrefabSystem::CanCreateChild(parentEntity, *registry)) {
                    LOG_WARN("[Prefab] Prefab instance hierarchy is locked. Use Unpack before adding children.");
                    ImGui::EndDragDropTarget();
                    return;
                }

                auto action = std::make_unique<CreateEntityAction>(
                    BuildSingleTextSnapshot(path.stem().string(), sourcePathStr),
                    parentEntity,
                    "Create Text From Asset");
                auto* actionPtr = action.get();
                UndoSystem::Instance().ExecuteAction(std::move(action), *registry);
                EditorSelection::Instance().SelectEntity(actionPtr->GetLiveRoot());
            }
            // Audio asset は AudioEmitter entity を生成する。
            else if (IsSupportedAudioAsset(sourcePathStr)) {
                if (!PrefabSystem::CanCreateChild(parentEntity, *registry)) {
                    LOG_WARN("[Prefab] Prefab instance hierarchy is locked. Use Unpack before adding children.");
                    ImGui::EndDragDropTarget();
                    return;
                }

                auto action = std::make_unique<CreateEntityAction>(
                    BuildAudioEmitterSnapshot(path.stem().string(), sourcePathStr, true),
                    parentEntity,
                    "Create Audio Source From Asset");
                auto* actionPtr = action.get();
                UndoSystem::Instance().ExecuteAction(std::move(action), *registry);
                EditorSelection::Instance().SelectEntity(actionPtr->GetLiveRoot());
            }
        }

        ImGui::EndDragDropTarget();
    }
}
