#include "UIEditor/UIEditorPanel.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <filesystem>
#include <functional>
#include <fstream>
#include <iterator>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include <DirectXMath.h>
#include <imgui.h>

#include "Archetype/Archetype.h"
#include "Asset/PrefabSystem.h"
#include "Component/CanvasItemComponent.h"
#include "Component/HierarchyComponent.h"
#include "Component/NameComponent.h"
#include "Component/PrefabInstanceComponent.h"
#include "Component/RectTransformComponent.h"
#include "Component/SpriteComponent.h"
#include "Component/TextComponent.h"
#include "Component/TransformComponent.h"
#include "Console/Logger.h"
#include "Engine/Editor2DEntityUtils.h"
#include "Engine/EditorSelection.h"
#include "Gameplay/HealthComponent.h"
#include "Gameplay/HPGaugeComponent.h"
#include "Hierarchy/HierarchySystem.h"
#include "Icon/IconFontManager.h"
#include "Registry/Registry.h"
#include "System/PathResolver.h"
#include "System/UndoSystem.h"
#include "Undo/ComponentUndoAction.h"
#include "Undo/EntitySnapshot.h"
#include "Undo/EntityUndoActions.h"

namespace {
    constexpr const char* kCanvasName = "BattleHUD_Canvas";
    constexpr const char* kWhiteTexture = "Data/Texture/UI/White.png";
    constexpr const char* kDefaultFont = "Data/Font/ArialUni.ttf";
    constexpr const char* kPrefabDirectory = "Data/UI/Prefabs";
    constexpr DirectX::XMFLOAT2 kReferenceResolution = { 1920.0f, 1080.0f };

    struct TemplatePreset
    {
        const char* name;
        HPGaugeTargetMode targetMode;
        DirectX::XMFLOAT2 position;
        DirectX::XMFLOAT2 size;
        HPGaugeTextFormat textFormat;
        const char* label;
    };

    TemplatePreset GetTemplatePreset(UIEditorPanel::TemplateKind kind)
    {
        switch (kind) {
        case UIEditorPanel::TemplateKind::BossHP:
            return { "BossHP_Widget", HPGaugeTargetMode::FirstBoss, { 0.0f, 480.0f }, { 760.0f, 46.0f }, HPGaugeTextFormat::LabelCurrentMax, "BOSS" };
        case UIEditorPanel::TemplateKind::PlayerHP:
        default:
            return { "PlayerHP_Widget", HPGaugeTargetMode::FirstPlayer, { -720.0f, 470.0f }, { 440.0f, 56.0f }, HPGaugeTextFormat::CurrentMax, "HP" };
        }
    }

    TransformComponent MakeTransform(const DirectX::XMFLOAT2& position)
    {
        TransformComponent transform{};
        transform.localPosition = { position.x, position.y, 0.0f };
        transform.localScale = { 1.0f, 1.0f, 1.0f };
        transform.isDirty = true;
        return transform;
    }

    RectTransformComponent MakeRect(const DirectX::XMFLOAT2& position, const DirectX::XMFLOAT2& size)
    {
        RectTransformComponent rect{};
        rect.anchoredPosition = position;
        rect.sizeDelta = size;
        rect.pivot = { 0.5f, 0.5f };
        rect.anchorMin = { 0.5f, 0.5f };
        rect.anchorMax = { 0.5f, 0.5f };
        return rect;
    }

    CanvasItemComponent MakeCanvas(int order)
    {
        CanvasItemComponent canvas{};
        canvas.orderInLayer = order;
        canvas.pixelSnap = true;
        return canvas;
    }

    EntitySnapshot::Node MakeNode(uint32_t localId,
                                  uint32_t parentId,
                                  const std::string& name,
                                  const DirectX::XMFLOAT2& position,
                                  const DirectX::XMFLOAT2& size,
                                  int order)
    {
        EntitySnapshot::Node node;
        node.localID = localId;
        node.sourceEntity = Entity::NULL_ID;
        node.parentLocalID = parentId;
        node.externalParent = Entity::NULL_ID;

        std::get<std::optional<NameComponent>>(node.components) = NameComponent{ name };
        std::get<std::optional<TransformComponent>>(node.components) = MakeTransform(position);
        std::get<std::optional<HierarchyComponent>>(node.components) = HierarchyComponent{};
        std::get<std::optional<RectTransformComponent>>(node.components) = MakeRect(position, size);
        std::get<std::optional<CanvasItemComponent>>(node.components) = MakeCanvas(order);
        return node;
    }

    SpriteComponent MakeSprite(const DirectX::XMFLOAT4& tint)
    {
        SpriteComponent sprite{};
        sprite.textureAssetPath = kWhiteTexture;
        sprite.tint = tint;
        return sprite;
    }

    HPGaugeBindingComponent MakeBinding(HPGaugeTargetMode targetMode)
    {
        HPGaugeBindingComponent binding{};
        binding.targetMode = targetMode;
        binding.visibleWhenNoTarget = true;
        binding.currentHP = 100;
        binding.maxHP = 100;
        binding.targetRatio = 1.0f;
        binding.displayedRatio = 1.0f;
        binding.delayedRatio = 1.0f;
        return binding;
    }

    HPGaugeFillComponent MakeFill(bool delayed)
    {
        HPGaugeFillComponent fill{};
        fill.useDisplayedRatio = !delayed;
        fill.useDelayedRatio = delayed;
        fill.hideWhenNoTarget = false;
        fill.runtimeRatio = 1.0f;
        if (delayed) {
            fill.colorMode = HPGaugeColorMode::Fixed;
            fill.fixedColor = { 0.85f, 0.16f, 0.12f, 0.42f };
        }
        return fill;
    }

    TextComponent MakeText(float fontSize = 24.0f)
    {
        TextComponent text{};
        text.text = "100 / 100";
        text.fontAssetPath = kDefaultFont;
        text.fontSize = fontSize;
        text.color = { 1.0f, 1.0f, 1.0f, 0.96f };
        text.alignment = TextAlignment::Center;
        return text;
    }

    HPGaugeTextComponent MakeGaugeText(HPGaugeTextFormat format, const char* label)
    {
        HPGaugeTextComponent gaugeText{};
        gaugeText.format = format;
        gaugeText.label = label ? label : "HP";
        gaugeText.hideWhenNoTarget = false;
        return gaugeText;
    }

    EntitySnapshot::Snapshot BuildCanvasSnapshot()
    {
        EntitySnapshot::Snapshot snapshot;
        snapshot.rootLocalID = 0;

        EntitySnapshot::Node canvas = MakeNode(
            0,
            EntitySnapshot::kInvalidLocalID,
            kCanvasName,
            { 0.0f, 0.0f },
            kReferenceResolution,
            -100);
        if (auto& item = std::get<std::optional<CanvasItemComponent>>(canvas.components); item.has_value()) {
            item->interactable = false;
        }

        snapshot.nodes.push_back(std::move(canvas));
        return snapshot;
    }

    EntitySnapshot::Snapshot BuildTemplateSnapshot(UIEditorPanel::TemplateKind kind)
    {
        const TemplatePreset preset = GetTemplatePreset(kind);
        const float barHeight = (std::max)(18.0f, preset.size.y - 16.0f);
        const DirectX::XMFLOAT2 barSize = { preset.size.x, barHeight };

        EntitySnapshot::Snapshot snapshot;
        snapshot.rootLocalID = 0;

        EntitySnapshot::Node root = MakeNode(0, EntitySnapshot::kInvalidLocalID, preset.name, preset.position, preset.size, 100);
        std::get<std::optional<HPGaugeBindingComponent>>(root.components) = MakeBinding(preset.targetMode);

        EntitySnapshot::Node background = MakeNode(1, 0, "Background", { 0.0f, 0.0f }, barSize, 0);
        std::get<std::optional<SpriteComponent>>(background.components) = MakeSprite({ 0.02f, 0.02f, 0.025f, 0.72f });

        EntitySnapshot::Node damage = MakeNode(2, 0, "DamagePreview", { 0.0f, 0.0f }, barSize, 1);
        std::get<std::optional<SpriteComponent>>(damage.components) = MakeSprite({ 0.85f, 0.16f, 0.12f, 0.42f });
        std::get<std::optional<HPGaugeFillComponent>>(damage.components) = MakeFill(true);

        EntitySnapshot::Node fill = MakeNode(3, 0, "Fill", { 0.0f, 0.0f }, barSize, 2);
        std::get<std::optional<SpriteComponent>>(fill.components) = MakeSprite({ 0.18f, 0.86f, 0.36f, 0.94f });
        std::get<std::optional<HPGaugeFillComponent>>(fill.components) = MakeFill(false);

        EntitySnapshot::Node text = MakeNode(4, 0, "HP_Text", { 0.0f, -1.0f }, preset.size, 3);
        std::get<std::optional<TextComponent>>(text.components) = MakeText(kind == UIEditorPanel::TemplateKind::BossHP ? 22.0f : 24.0f);
        std::get<std::optional<HPGaugeTextComponent>>(text.components) = MakeGaugeText(preset.textFormat, preset.label);

        snapshot.nodes.push_back(std::move(root));
        snapshot.nodes.push_back(std::move(background));
        snapshot.nodes.push_back(std::move(damage));
        snapshot.nodes.push_back(std::move(fill));
        snapshot.nodes.push_back(std::move(text));
        return snapshot;
    }

    EntitySnapshot::Snapshot BuildPartSnapshot(UIEditorPanel::PartKind kind)
    {
        EntitySnapshot::Snapshot snapshot;
        snapshot.rootLocalID = 0;

        switch (kind) {
        case UIEditorPanel::PartKind::GaugeRoot: {
            EntitySnapshot::Node root = MakeNode(0, EntitySnapshot::kInvalidLocalID, "HPGauge_Widget", { 0.0f, 0.0f }, { 420.0f, 56.0f }, 100);
            std::get<std::optional<HPGaugeBindingComponent>>(root.components) = MakeBinding(HPGaugeTargetMode::FirstPlayer);
            snapshot.nodes.push_back(std::move(root));
            break;
        }
        case UIEditorPanel::PartKind::FillImage: {
            EntitySnapshot::Node fill = MakeNode(0, EntitySnapshot::kInvalidLocalID, "Fill", { 0.0f, 0.0f }, { 360.0f, 28.0f }, 2);
            std::get<std::optional<SpriteComponent>>(fill.components) = MakeSprite({ 0.18f, 0.86f, 0.36f, 0.94f });
            std::get<std::optional<HPGaugeFillComponent>>(fill.components) = MakeFill(false);
            snapshot.nodes.push_back(std::move(fill));
            break;
        }
        case UIEditorPanel::PartKind::DamagePreview: {
            EntitySnapshot::Node damage = MakeNode(0, EntitySnapshot::kInvalidLocalID, "DamagePreview", { 0.0f, 0.0f }, { 360.0f, 28.0f }, 1);
            std::get<std::optional<SpriteComponent>>(damage.components) = MakeSprite({ 0.85f, 0.16f, 0.12f, 0.42f });
            std::get<std::optional<HPGaugeFillComponent>>(damage.components) = MakeFill(true);
            snapshot.nodes.push_back(std::move(damage));
            break;
        }
        case UIEditorPanel::PartKind::HPText: {
            EntitySnapshot::Node text = MakeNode(0, EntitySnapshot::kInvalidLocalID, "HP_Text", { 0.0f, 0.0f }, { 360.0f, 40.0f }, 3);
            std::get<std::optional<TextComponent>>(text.components) = MakeText();
            std::get<std::optional<HPGaugeTextComponent>>(text.components) = MakeGaugeText(HPGaugeTextFormat::CurrentMax, "HP");
            snapshot.nodes.push_back(std::move(text));
            break;
        }
        case UIEditorPanel::PartKind::Image:
        default: {
            EntitySnapshot::Node image = MakeNode(0, EntitySnapshot::kInvalidLocalID, "Image", { 0.0f, 0.0f }, { 360.0f, 28.0f }, 0);
            std::get<std::optional<SpriteComponent>>(image.components) = MakeSprite({ 0.08f, 0.09f, 0.11f, 0.80f });
            snapshot.nodes.push_back(std::move(image));
            break;
        }
        }

        return snapshot;
    }

    EntityID ExecuteCreateSnapshot(Registry& registry,
                                   EntitySnapshot::Snapshot snapshot,
                                   EntityID parent,
                                   const char* actionName)
    {
        if (snapshot.nodes.empty()) {
            return Entity::NULL_ID;
        }
        if (!Entity::IsNull(parent) && !PrefabSystem::CanCreateChild(parent, registry)) {
            LOG_WARN("[UIEditor] Prefab instance hierarchy is locked. Use Unpack before adding children.");
            return Entity::NULL_ID;
        }

        auto action = std::make_unique<CreateEntityAction>(std::move(snapshot), parent, actionName);
        auto* actionPtr = action.get();
        UndoSystem::Instance().ExecuteAction(std::move(action), registry);
        const EntityID root = actionPtr->GetLiveRoot();
        if (!Entity::IsNull(root) && registry.IsAlive(root)) {
            Editor2D::FinalizeCreatedEntity(registry, root);
        }
        return root;
    }

    EntityID GetParent(Registry& registry, EntityID entity)
    {
        if (auto* hierarchy = registry.GetComponent<HierarchyComponent>(entity)) {
            return hierarchy->parent;
        }
        return Entity::NULL_ID;
    }

    std::string GetName(Registry& registry, EntityID entity, const char* fallback = "None")
    {
        if (Entity::IsNull(entity) || !registry.IsAlive(entity)) {
            return fallback;
        }
        if (auto* name = registry.GetComponent<NameComponent>(entity)) {
            return name->name;
        }
        return std::string("Entity ") + std::to_string(static_cast<unsigned long long>(entity));
    }

    std::filesystem::path PrefabDirectory()
    {
        return std::filesystem::path(PathResolver::Resolve(kPrefabDirectory));
    }

    std::string ReadText(const std::filesystem::path& path)
    {
        std::ifstream stream(path, std::ios::binary);
        if (!stream.is_open()) {
            return {};
        }
        return std::string(std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>());
    }

    EntitySnapshot::Snapshot BuildPrefabInstanceSnapshot(const std::filesystem::path& prefabPath)
    {
        EntitySnapshot::Snapshot snapshot;
        if (!PrefabSystem::LoadPrefabSnapshot(prefabPath, snapshot)) {
            return snapshot;
        }

        for (auto& node : snapshot.nodes) {
            if (node.localID != snapshot.rootLocalID) {
                continue;
            }
            PrefabInstanceComponent prefab{};
            prefab.prefabAssetPath = prefabPath.generic_string();
            prefab.hasOverrides = false;
            std::get<std::optional<PrefabInstanceComponent>>(node.components) = prefab;
            break;
        }
        return snapshot;
    }

    std::vector<std::filesystem::path> CollectPrefabs()
    {
        std::vector<std::filesystem::path> paths;
        const std::filesystem::path dir = PrefabDirectory();

        std::error_code ec;
        if (!std::filesystem::exists(dir, ec)) {
            return paths;
        }

        for (std::filesystem::directory_iterator it(dir, ec), end; !ec && it != end; it.increment(ec)) {
            if (it->is_regular_file(ec) && it->path().extension() == ".prefab") {
                paths.push_back(it->path());
            }
        }
        std::sort(paths.begin(), paths.end());
        return paths;
    }

    template <typename T>
    void RecordComponentChange(Registry& registry, EntityID entity, const T& before, const T& after)
    {
        UndoSystem::Instance().RecordAction(std::make_unique<ComponentUndoAction<T>>(entity, before, after));
        PrefabSystem::MarkPrefabOverride(entity, registry);
    }

    std::vector<EntityID> CollectSubtree(Registry& registry, EntityID root)
    {
        std::vector<EntityID> entities;
        EntitySnapshot::CollectHierarchy(root, registry, entities);
        return entities;
    }

    EntityID FindFirstInSubtreeWithFill(Registry& registry, EntityID root)
    {
        for (EntityID entity : CollectSubtree(registry, root)) {
            if (registry.GetComponent<HPGaugeFillComponent>(entity)) {
                return entity;
            }
        }
        return Entity::NULL_ID;
    }

    EntityID FindFirstInSubtreeWithText(Registry& registry, EntityID root)
    {
        for (EntityID entity : CollectSubtree(registry, root)) {
            if (registry.GetComponent<HPGaugeTextComponent>(entity)) {
                return entity;
            }
        }
        return Entity::NULL_ID;
    }

    DirectX::XMFLOAT2 PresetPosition(UIEditorPanel::PlacementPreset preset)
    {
        constexpr float margin = 48.0f;
        const float halfW = kReferenceResolution.x * 0.5f;
        const float halfH = kReferenceResolution.y * 0.5f;
        switch (preset) {
        case UIEditorPanel::PlacementPreset::TopCenter: return { 0.0f, halfH - margin };
        case UIEditorPanel::PlacementPreset::TopRight: return { halfW - 260.0f, halfH - margin };
        case UIEditorPanel::PlacementPreset::MiddleLeft: return { -halfW + 260.0f, 0.0f };
        case UIEditorPanel::PlacementPreset::MiddleRight: return { halfW - 260.0f, 0.0f };
        case UIEditorPanel::PlacementPreset::BottomLeft: return { -halfW + 260.0f, -halfH + margin };
        case UIEditorPanel::PlacementPreset::BottomRight: return { halfW - 260.0f, -halfH + margin };
        case UIEditorPanel::PlacementPreset::TopLeft:
        default: return { -halfW + 260.0f, halfH - margin };
        }
    }

    ImVec2 ToCanvasPoint(const ImVec2& origin, float scale, const RectTransformComponent& rect)
    {
        return ImVec2(
            origin.x + kReferenceResolution.x * 0.5f * scale + rect.anchoredPosition.x * scale,
            origin.y + kReferenceResolution.y * 0.5f * scale - rect.anchoredPosition.y * scale);
    }
}

void UIEditorPanel::DrawWorkspace(Registry* registry, bool* outFocused)
{
    m_registry = registry;
    if (outFocused) {
        *outFocused = false;
    }

    ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBringToFrontOnFocus;

    if (!ImGui::Begin("UI Editor Workspace", nullptr, flags)) {
        if (outFocused) {
            *outFocused = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);
        }
        ImGui::End();
        return;
    }

    if (outFocused) {
        *outFocused = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);
    }

    DrawToolbar();

    const float prefabHeight = 72.0f;
    const ImVec2 available = ImGui::GetContentRegionAvail();
    const float mainHeight = (std::max)(220.0f, available.y - prefabHeight);
    const float paletteWidth = 230.0f;
    const float propertiesWidth = 300.0f;
    const float spacing = ImGui::GetStyle().ItemSpacing.x;

    ImGui::BeginChild("##UIEditorMain", ImVec2(0.0f, mainHeight), true, ImGuiWindowFlags_NoScrollbar);
    DrawPalette();
    ImGui::SameLine(0.0f, spacing);

    ImGui::BeginChild("##UIEditorCenter", ImVec2((std::max)(260.0f, ImGui::GetContentRegionAvail().x - propertiesWidth - spacing), 0.0f), false);
    DrawDesignerView();
    DrawWidgetTree();
    ImGui::EndChild();

    ImGui::SameLine(0.0f, spacing);
    ImGui::BeginChild("##UIEditorProperties", ImVec2(propertiesWidth, 0.0f), true);
    DrawProperties();
    ImGui::EndChild();

    ImGui::EndChild();

    ImGui::BeginChild("##UIEditorPrefabBar", ImVec2(0.0f, 0.0f), true);
    DrawPrefabBar();
    ImGui::EndChild();

    ImGui::End();
}

void UIEditorPanel::DrawToolbar()
{
    const EntityID canvas = FindCanvas();
    ImGui::TextUnformatted(ICON_FA_LAYER_GROUP " UI Editor");
    ImGui::SameLine();
    ImGui::TextDisabled("|");
    ImGui::SameLine();
    ImGui::Text("Canvas: %s", (m_registry ? GetName(*m_registry, canvas, "Not created").c_str() : "No Registry"));
    ImGui::SameLine();
    ImGui::TextDisabled("Resolution: 1920 x 1080");
    ImGui::SameLine();
    ImGui::Checkbox("Snap", &m_pixelSnap);
    ImGui::SameLine();
    ImGui::Checkbox("Safe Area", &m_showSafeArea);
    ImGui::SameLine();
    ImGui::Checkbox("Grid", &m_showGrid);
    ImGui::Separator();
}

void UIEditorPanel::DrawPalette()
{
    ImGui::BeginChild("##UIEditorPalette", ImVec2(230.0f, 0.0f), true);
    ImGui::TextUnformatted("Canvas");
    if (ImGui::Button("Create / Select Canvas", ImVec2(-1.0f, 0.0f))) {
        SelectEntity(FindOrCreateCanvas());
    }

    ImGui::Spacing();
    ImGui::TextUnformatted("Templates");
    if (ImGui::Button("Template...", ImVec2(-1.0f, 0.0f))) {
        ImGui::OpenPopup("UIEditorTemplatePopup");
    }
    if (ImGui::BeginPopup("UIEditorTemplatePopup")) {
        if (ImGui::MenuItem("Player HP")) {
            CreateTemplate(TemplateKind::PlayerHP);
        }
        if (ImGui::MenuItem("Boss HP")) {
            CreateTemplate(TemplateKind::BossHP);
        }
        ImGui::EndPopup();
    }

    ImGui::Spacing();
    ImGui::TextUnformatted("Parts");
    if (ImGui::Button("Gauge Root", ImVec2(-1.0f, 0.0f))) {
        CreatePart(PartKind::GaugeRoot);
    }
    if (ImGui::Button("Image", ImVec2(-1.0f, 0.0f))) {
        CreatePart(PartKind::Image);
    }
    if (ImGui::Button("Fill Image", ImVec2(-1.0f, 0.0f))) {
        CreatePart(PartKind::FillImage);
    }
    if (ImGui::Button("Damage Preview", ImVec2(-1.0f, 0.0f))) {
        CreatePart(PartKind::DamagePreview);
    }
    if (ImGui::Button("HP Text", ImVec2(-1.0f, 0.0f))) {
        CreatePart(PartKind::HPText);
    }

    ImGui::Spacing();
    ImGui::TextUnformatted("Saved Prefabs");
    const std::vector<std::filesystem::path> prefabs = CollectPrefabs();
    if (prefabs.empty()) {
        ImGui::TextDisabled("No prefabs in Data/UI/Prefabs.");
    } else {
        for (const std::filesystem::path& path : prefabs) {
            if (ImGui::SmallButton(path.filename().string().c_str())) {
                InstantiatePrefab(path);
            }
        }
    }

    ImGui::EndChild();
}

void UIEditorPanel::DrawDesignerView()
{
    const float treeHeight = 150.0f;
    const ImVec2 avail = ImGui::GetContentRegionAvail();
    ImGui::BeginChild("##UIEditorDesigner", ImVec2(0.0f, (std::max)(160.0f, avail.y - treeHeight)), true, ImGuiWindowFlags_NoScrollbar);
    ImGui::TextUnformatted("Designer View");

    const ImVec2 canvasAvail = ImGui::GetContentRegionAvail();
    const float scale = (std::min)(canvasAvail.x / kReferenceResolution.x, (canvasAvail.y - 8.0f) / kReferenceResolution.y);
    const ImVec2 canvasSize(kReferenceResolution.x * scale, kReferenceResolution.y * scale);
    const ImVec2 origin(
        ImGui::GetCursorScreenPos().x + (canvasAvail.x - canvasSize.x) * 0.5f,
        ImGui::GetCursorScreenPos().y + (canvasAvail.y - canvasSize.y) * 0.5f);

    ImDrawList* draw = ImGui::GetWindowDrawList();
    draw->AddRectFilled(origin, ImVec2(origin.x + canvasSize.x, origin.y + canvasSize.y), IM_COL32(22, 24, 29, 255));
    draw->AddRect(origin, ImVec2(origin.x + canvasSize.x, origin.y + canvasSize.y), IM_COL32(90, 96, 112, 255));

    if (m_showSafeArea) {
        const float marginX = 64.0f * scale;
        const float marginY = 36.0f * scale;
        draw->AddRect(
            ImVec2(origin.x + marginX, origin.y + marginY),
            ImVec2(origin.x + canvasSize.x - marginX, origin.y + canvasSize.y - marginY),
            IM_COL32(80, 170, 255, 120));
    }

    if (m_showGrid) {
        constexpr float grid = 120.0f;
        for (float x = grid; x < kReferenceResolution.x; x += grid) {
            const float sx = origin.x + x * scale;
            draw->AddLine(ImVec2(sx, origin.y), ImVec2(sx, origin.y + canvasSize.y), IM_COL32(255, 255, 255, 18));
        }
        for (float y = grid; y < kReferenceResolution.y; y += grid) {
            const float sy = origin.y + y * scale;
            draw->AddLine(ImVec2(origin.x, sy), ImVec2(origin.x + canvasSize.x, sy), IM_COL32(255, 255, 255, 18));
        }
    }

    if (m_registry) {
        const EntityID canvas = FindCanvas();
        if (!Entity::IsNull(canvas)) {
            for (EntityID entity : CollectSubtree(*m_registry, canvas)) {
                if (entity == canvas) {
                    continue;
                }
                auto* rect = m_registry->GetComponent<RectTransformComponent>(entity);
                if (!rect) {
                    continue;
                }

                const ImVec2 center = ToCanvasPoint(origin, scale, *rect);
                const ImVec2 size(rect->sizeDelta.x * scale, rect->sizeDelta.y * scale);
                const ImVec2 rectMin(center.x - size.x * rect->pivot.x, center.y - size.y * (1.0f - rect->pivot.y));
                const ImVec2 rectMax(rectMin.x + size.x, rectMin.y + size.y);
                const bool selected = entity == m_selectedEntity || entity == m_selectedGaugeRoot;
                const ImU32 color = m_registry->GetComponent<HPGaugeBindingComponent>(entity)
                    ? IM_COL32(80, 180, 100, selected ? 120 : 70)
                    : (m_registry->GetComponent<TextComponent>(entity) ? IM_COL32(230, 230, 255, selected ? 105 : 55) : IM_COL32(110, 160, 255, selected ? 105 : 55));
                draw->AddRectFilled(rectMin, rectMax, color);
                draw->AddRect(rectMin, rectMax, selected ? IM_COL32(255, 220, 90, 255) : IM_COL32(170, 190, 220, 170), selected ? 0.0f : 0.0f, 0, selected ? 2.0f : 1.0f);

                ImGui::SetCursorScreenPos(rectMin);
                ImGui::PushID(static_cast<int>(Entity::GetIndex(entity)));
                if (ImGui::InvisibleButton("##designerItem", size)) {
                    SelectEntity(entity);
                }
                ImGui::PopID();
            }
        } else {
            const char* text = "Create a Canvas, then add a Template or Parts.";
            const ImVec2 textSize = ImGui::CalcTextSize(text);
            draw->AddText(ImVec2(origin.x + (canvasSize.x - textSize.x) * 0.5f, origin.y + (canvasSize.y - textSize.y) * 0.5f), IM_COL32(190, 195, 205, 255), text);
        }
    }

    ImGui::SetCursorScreenPos(ImVec2(origin.x, origin.y + canvasSize.y + 2.0f));
    ImGui::Dummy(ImVec2(canvasSize.x, 1.0f));
    ImGui::EndChild();
}

void UIEditorPanel::DrawWidgetTree()
{
    ImGui::BeginChild("##UIEditorWidgetTree", ImVec2(0.0f, 0.0f), true);
    ImGui::TextUnformatted("Widget Tree");
    if (!m_registry) {
        ImGui::TextDisabled("No registry.");
        ImGui::EndChild();
        return;
    }

    const EntityID canvas = FindCanvas();
    if (Entity::IsNull(canvas)) {
        ImGui::TextDisabled("No Canvas.");
        ImGui::EndChild();
        return;
    }

    std::function<void(EntityID)> drawNode = [&](EntityID entity) {
        const bool selected = entity == m_selectedEntity;
        auto* hierarchy = m_registry->GetComponent<HierarchyComponent>(entity);
        const bool hasChildren = hierarchy && !Entity::IsNull(hierarchy->firstChild);
        ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_DefaultOpen;
        if (!hasChildren) {
            flags |= ImGuiTreeNodeFlags_Leaf;
        }
        if (selected) {
            flags |= ImGuiTreeNodeFlags_Selected;
        }

        const bool open = ImGui::TreeNodeEx(reinterpret_cast<void*>(static_cast<uintptr_t>(entity)), flags, "%s", GetName(*m_registry, entity).c_str());
        if (ImGui::IsItemClicked()) {
            SelectEntity(entity);
        }
        if (open) {
            if (hierarchy) {
                EntityID child = hierarchy->firstChild;
                while (!Entity::IsNull(child)) {
                    EntityID next = Entity::NULL_ID;
                    if (auto* childHierarchy = m_registry->GetComponent<HierarchyComponent>(child)) {
                        next = childHierarchy->nextSibling;
                    }
                    drawNode(child);
                    child = next;
                }
            }
            ImGui::TreePop();
        }
    };

    drawNode(canvas);
    ImGui::EndChild();
}

void UIEditorPanel::DrawProperties()
{
    ImGui::TextUnformatted("Properties");
    if (!m_registry) {
        ImGui::TextDisabled("No registry.");
        return;
    }

    if (Entity::IsNull(m_selectedEntity) || !m_registry->IsAlive(m_selectedEntity)) {
        ImGui::TextDisabled("Select a UI element.");
        return;
    }

    ImGui::TextWrapped("%s", GetName(*m_registry, m_selectedEntity).c_str());
    if (auto* name = m_registry->GetComponent<NameComponent>(m_selectedEntity)) {
        char buffer[128] = {};
        std::snprintf(buffer, sizeof(buffer), "%s", name->name.c_str());
        if (ImGui::InputText("Name", buffer, sizeof(buffer))) {
            const NameComponent before = *name;
            name->name = buffer;
            RecordComponentChange(*m_registry, m_selectedEntity, before, *name);
        }
    }

    if (auto* rect = m_registry->GetComponent<RectTransformComponent>(m_selectedEntity)) {
        ImGui::SeparatorText("Layout");
        const char* presetNames[] = { "Top Left", "Top Center", "Top Right", "Middle Left", "Middle Right", "Bottom Left", "Bottom Right", "Custom" };
        int presetIndex = static_cast<int>(m_lastPlacementPreset);
        if (ImGui::Combo("Placement", &presetIndex, presetNames, IM_ARRAYSIZE(presetNames))) {
            m_lastPlacementPreset = static_cast<PlacementPreset>(presetIndex);
            if (m_lastPlacementPreset != PlacementPreset::Custom) {
                ApplyPlacementPreset(m_selectedEntity, m_lastPlacementPreset);
            }
        }

        RectTransformComponent before = *rect;
        bool changed = false;
        changed |= ImGui::DragFloat2("Position", &rect->anchoredPosition.x, 1.0f);
        changed |= ImGui::DragFloat2("Size", &rect->sizeDelta.x, 1.0f, 1.0f, 4096.0f);
        changed |= ImGui::DragFloat2("Pivot", &rect->pivot.x, 0.01f, 0.0f, 1.0f);
        if (changed) {
            if (auto* transform = m_registry->GetComponent<TransformComponent>(m_selectedEntity)) {
                Editor2D::SyncRectTransformToTransform(*rect, *transform);
            }
            m_lastPlacementPreset = PlacementPreset::Custom;
            RecordComponentChange(*m_registry, m_selectedEntity, before, *rect);
        }
    }

    if (auto* binding = m_registry->GetComponent<HPGaugeBindingComponent>(m_selectedEntity)) {
        ImGui::SeparatorText("HP Gauge Binding");
        const char* modes[] = { "Explicit", "First Player", "First Boss" };
        int mode = 0;
        if (binding->targetMode == HPGaugeTargetMode::FirstPlayer) {
            mode = 1;
        } else if (binding->targetMode == HPGaugeTargetMode::FirstBoss) {
            mode = 2;
        }

        if (ImGui::Combo("Target Mode", &mode, modes, IM_ARRAYSIZE(modes))) {
            const HPGaugeBindingComponent before = *binding;
            binding->targetMode = (mode == 1) ? HPGaugeTargetMode::FirstPlayer : (mode == 2 ? HPGaugeTargetMode::FirstBoss : HPGaugeTargetMode::Explicit);
            RecordComponentChange(*m_registry, m_selectedEntity, before, *binding);
        }

        const EntityID primary = EditorSelection::Instance().GetPrimaryEntity();
        ImGui::Text("Explicit: %s", GetName(*m_registry, binding->explicitTarget, "None").c_str());
        ImGui::BeginDisabled(Entity::IsNull(primary) || !m_registry->IsAlive(primary) || !m_registry->GetComponent<HealthComponent>(primary));
        if (ImGui::Button("Pick Scene Selection", ImVec2(-1.0f, 0.0f))) {
            const HPGaugeBindingComponent before = *binding;
            binding->targetMode = HPGaugeTargetMode::Explicit;
            binding->explicitTarget = primary;
            RecordComponentChange(*m_registry, m_selectedEntity, before, *binding);
        }
        ImGui::EndDisabled();

        HPGaugeBindingComponent before = *binding;
        bool changed = false;
        changed |= ImGui::Checkbox("Visible When No Target", &binding->visibleWhenNoTarget);
        changed |= ImGui::Checkbox("Hide When Dead", &binding->hideWhenDead);
        changed |= ImGui::Checkbox("Hide When Full", &binding->hideWhenFull);
        changed |= ImGui::DragFloat("Smoothing", &binding->smoothingSpeed, 0.1f, 0.0f, 120.0f);
        changed |= ImGui::DragFloat("Damage Delay", &binding->damagePreviewDelay, 0.01f, 0.0f, 5.0f);
        changed |= ImGui::DragFloat("Damage Speed", &binding->damagePreviewSpeed, 0.1f, 0.0f, 120.0f);
        if (changed) {
            RecordComponentChange(*m_registry, m_selectedEntity, before, *binding);
        }

        ImGui::SeparatorText("Preview HP");
        const std::array<std::pair<const char*, float>, 5> ratios = {
            std::pair<const char*, float>{ "100%", 1.0f },
            std::pair<const char*, float>{ "75%", 0.75f },
            std::pair<const char*, float>{ "50%", 0.50f },
            std::pair<const char*, float>{ "25%", 0.25f },
            std::pair<const char*, float>{ "0%", 0.0f }
        };
        for (size_t i = 0; i < ratios.size(); ++i) {
            if (i > 0) {
                ImGui::SameLine();
            }
            if (ImGui::SmallButton(ratios[i].first)) {
                const HPGaugeBindingComponent previewBefore = *binding;
                binding->targetValid = true;
                binding->maxHP = 100;
                binding->currentHP = static_cast<int>(std::round(ratios[i].second * 100.0f));
                binding->targetRatio = ratios[i].second;
                binding->displayedRatio = ratios[i].second;
                binding->delayedRatio = 1.0f;
                RecordComponentChange(*m_registry, m_selectedEntity, previewBefore, *binding);
            }
        }
    }

    if (auto* sprite = m_registry->GetComponent<SpriteComponent>(m_selectedEntity)) {
        ImGui::SeparatorText("Image");
        SpriteComponent before = *sprite;
        bool changed = false;
        changed |= ImGui::ColorEdit4("Tint", &sprite->tint.x);
        if (changed) {
            RecordComponentChange(*m_registry, m_selectedEntity, before, *sprite);
        }
    }

    if (auto* fill = m_registry->GetComponent<HPGaugeFillComponent>(m_selectedEntity)) {
        ImGui::SeparatorText("Fill");
        const char* directions[] = { "Left To Right", "Right To Left", "Bottom To Top", "Top To Bottom" };
        int direction = static_cast<int>(fill->fillDirection);
        if (ImGui::Combo("Direction", &direction, directions, IM_ARRAYSIZE(directions))) {
            const HPGaugeFillComponent before = *fill;
            fill->fillDirection = static_cast<HPGaugeFillDirection>(direction);
            RecordComponentChange(*m_registry, m_selectedEntity, before, *fill);
        }

        const char* colorModes[] = { "Fixed", "Threshold", "Gradient" };
        int colorMode = static_cast<int>(fill->colorMode);
        if (ImGui::Combo("Color Mode", &colorMode, colorModes, IM_ARRAYSIZE(colorModes))) {
            const HPGaugeFillComponent before = *fill;
            fill->colorMode = static_cast<HPGaugeColorMode>(colorMode);
            RecordComponentChange(*m_registry, m_selectedEntity, before, *fill);
        }

        HPGaugeFillComponent before = *fill;
        bool changed = false;
        changed |= ImGui::ColorEdit4("Fixed", &fill->fixedColor.x);
        changed |= ImGui::ColorEdit4("High", &fill->highColor.x);
        changed |= ImGui::ColorEdit4("Mid", &fill->midColor.x);
        changed |= ImGui::ColorEdit4("Low", &fill->lowColor.x);
        if (changed) {
            RecordComponentChange(*m_registry, m_selectedEntity, before, *fill);
        }
    }

    if (auto* text = m_registry->GetComponent<TextComponent>(m_selectedEntity)) {
        ImGui::SeparatorText("Text");
        TextComponent before = *text;
        bool changed = false;
        changed |= ImGui::DragFloat("Font Size", &text->fontSize, 0.5f, 4.0f, 128.0f);
        changed |= ImGui::ColorEdit4("Color", &text->color.x);
        if (changed) {
            RecordComponentChange(*m_registry, m_selectedEntity, before, *text);
        }
    }

    if (auto* gaugeText = m_registry->GetComponent<HPGaugeTextComponent>(m_selectedEntity)) {
        ImGui::SeparatorText("HP Text Binding");
        const char* formats[] = { "Current / Max", "Current Only", "Percent", "Label + Current / Max" };
        int format = static_cast<int>(gaugeText->format);
        if (ImGui::Combo("Format", &format, formats, IM_ARRAYSIZE(formats))) {
            const HPGaugeTextComponent before = *gaugeText;
            gaugeText->format = static_cast<HPGaugeTextFormat>(format);
            RecordComponentChange(*m_registry, m_selectedEntity, before, *gaugeText);
        }
    }
}

void UIEditorPanel::DrawPrefabBar()
{
    const EntityID root = FindSelectedGaugeRoot();
    const bool hasRoot = !Entity::IsNull(root) && m_registry && m_registry->IsAlive(root);
    ImGui::Text("Selected Widget: %s", (hasRoot ? GetName(*m_registry, root).c_str() : "None"));

    ImGui::BeginDisabled(!hasRoot);
    if (ImGui::Button("Save as HP Gauge Prefab")) {
        SaveSelectedAsPrefab();
    }
    ImGui::SameLine();
    if (ImGui::Button("Apply")) {
        ApplySelectedPrefab();
    }
    ImGui::SameLine();
    if (ImGui::Button("Revert")) {
        RevertSelectedPrefab();
    }
    ImGui::SameLine();
    if (ImGui::Button("Unpack")) {
        UnpackSelectedPrefab();
    }
    ImGui::EndDisabled();

    if (!m_lastPrefabPath.empty()) {
        ImGui::SameLine();
        ImGui::TextDisabled("%s", m_lastPrefabPath.string().c_str());
    }
}

EntityID UIEditorPanel::FindOrCreateCanvas()
{
    if (!m_registry) {
        return Entity::NULL_ID;
    }

    const EntityID existing = FindCanvas();
    if (!Entity::IsNull(existing)) {
        return existing;
    }

    return ExecuteCreateSnapshot(*m_registry, BuildCanvasSnapshot(), Entity::NULL_ID, "Create UI Canvas");
}

EntityID UIEditorPanel::CreateTemplate(TemplateKind kind)
{
    if (!m_registry) {
        return Entity::NULL_ID;
    }

    const EntityID canvas = FindOrCreateCanvas();
    if (Entity::IsNull(canvas)) {
        return Entity::NULL_ID;
    }

    EntityID root = ExecuteCreateSnapshot(*m_registry, BuildTemplateSnapshot(kind), canvas, "Create HP Gauge Template");
    if (!Entity::IsNull(root)) {
        SelectEntity(root);
    }
    return root;
}

EntityID UIEditorPanel::CreatePart(PartKind kind)
{
    if (!m_registry) {
        return Entity::NULL_ID;
    }

    if (kind == PartKind::Canvas) {
        return FindOrCreateCanvas();
    }

    const EntityID canvas = FindOrCreateCanvas();
    if (Entity::IsNull(canvas)) {
        return Entity::NULL_ID;
    }

    EntityID parent = canvas;
    if (kind == PartKind::FillImage || kind == PartKind::DamagePreview || kind == PartKind::HPText) {
        parent = FindSelectedGaugeRoot();
        if (Entity::IsNull(parent)) {
            LOG_WARN("[UIEditor] Select or create a Gauge Root before adding HP-bound parts.");
            return Entity::NULL_ID;
        }
    } else if (kind == PartKind::Image) {
        const EntityID root = FindSelectedGaugeRoot();
        parent = Entity::IsNull(root) ? canvas : root;
    }

    EntityID entity = ExecuteCreateSnapshot(*m_registry, BuildPartSnapshot(kind), parent, "Create UI Part");
    if (!Entity::IsNull(entity)) {
        SelectEntity(entity);
    }
    return entity;
}

EntityID UIEditorPanel::InstantiatePrefab(const std::filesystem::path& path)
{
    if (!m_registry) {
        return Entity::NULL_ID;
    }

    const EntityID canvas = FindOrCreateCanvas();
    if (Entity::IsNull(canvas)) {
        return Entity::NULL_ID;
    }

    EntityID root = PrefabSystem::InstantiatePrefab(path, *m_registry, canvas);
    if (!Entity::IsNull(root) && m_registry->IsAlive(root)) {
        EntitySnapshot::Snapshot snapshot = EntitySnapshot::CaptureSubtree(root, *m_registry);
        auto action = std::make_unique<CreateEntityAction>(std::move(snapshot), canvas, "Instantiate UI Prefab");
        action->AdoptLiveRoot(root);
        UndoSystem::Instance().RecordAction(std::move(action));
        m_lastPrefabPath = path;
        SelectEntity(root);
    }
    return root;
}

bool UIEditorPanel::SaveSelectedAsPrefab()
{
    if (!m_registry) {
        return false;
    }

    const EntityID root = FindSelectedGaugeRoot();
    if (Entity::IsNull(root)) {
        LOG_WARN("[UIEditor] Select an HP Gauge Root before saving a prefab.");
        return false;
    }

    std::filesystem::path outPath;
    if (!PrefabSystem::SaveEntityAsPrefab(root, *m_registry, PrefabDirectory(), &outPath)) {
        LOG_WARN("[UIEditor] Failed to save HP Gauge prefab.");
        return false;
    }

    std::optional<PrefabInstanceComponent> before;
    if (auto* existing = m_registry->GetComponent<PrefabInstanceComponent>(root)) {
        before = *existing;
    }

    PrefabInstanceComponent prefab{};
    prefab.prefabAssetPath = outPath.generic_string();
    prefab.hasOverrides = false;
    m_registry->AddComponent(root, prefab);
    UndoSystem::Instance().RecordAction(
        std::make_unique<OptionalComponentUndoAction<PrefabInstanceComponent>>(
            root,
            before,
            prefab,
            "Bind UI Prefab"));

    m_lastPrefabPath = outPath;
    LOG_INFO("[UIEditor] Saved HP Gauge prefab: %s", outPath.string().c_str());
    return true;
}

bool UIEditorPanel::ApplySelectedPrefab()
{
    if (!m_registry) {
        return false;
    }

    const EntityID root = FindSelectedGaugeRoot();
    auto* prefab = Entity::IsNull(root) ? nullptr : m_registry->GetComponent<PrefabInstanceComponent>(root);
    if (!prefab) {
        return false;
    }

    const std::filesystem::path path = prefab->prefabAssetPath;
    const std::string beforeText = ReadText(path);
    const bool oldHasOverrides = prefab->hasOverrides;
    if (!PrefabSystem::ApplyPrefab(root, *m_registry)) {
        return false;
    }

    const std::string afterText = ReadText(path);
    UndoSystem::Instance().RecordAction(
        std::make_unique<ApplyPrefabAction>(
            root,
            path,
            beforeText,
            afterText,
            oldHasOverrides,
            prefab->hasOverrides));
    m_lastPrefabPath = path;
    return true;
}

bool UIEditorPanel::RevertSelectedPrefab()
{
    if (!m_registry) {
        return false;
    }

    const EntityID root = FindSelectedGaugeRoot();
    auto* prefab = Entity::IsNull(root) ? nullptr : m_registry->GetComponent<PrefabInstanceComponent>(root);
    if (!prefab) {
        return false;
    }

    const EntityID parent = GetParent(*m_registry, root);
    EntitySnapshot::Snapshot before = EntitySnapshot::CaptureSubtree(root, *m_registry);
    EntitySnapshot::Snapshot after = BuildPrefabInstanceSnapshot(prefab->prefabAssetPath);
    if (before.nodes.empty() || after.nodes.empty()) {
        return false;
    }

    auto action = std::make_unique<ReplaceEntitySubtreeAction>(
        std::move(before),
        std::move(after),
        root,
        parent,
        "Revert UI Prefab");
    auto* actionPtr = action.get();
    UndoSystem::Instance().ExecuteAction(std::move(action), *m_registry);
    SelectEntity(actionPtr->GetLiveRoot());
    return true;
}

bool UIEditorPanel::UnpackSelectedPrefab()
{
    if (!m_registry) {
        return false;
    }

    const EntityID root = FindSelectedGaugeRoot();
    auto* prefab = Entity::IsNull(root) ? nullptr : m_registry->GetComponent<PrefabInstanceComponent>(root);
    if (!prefab) {
        return false;
    }

    UndoSystem::Instance().ExecuteAction(
        std::make_unique<OptionalComponentUndoAction<PrefabInstanceComponent>>(
            root,
            std::optional<PrefabInstanceComponent>(*prefab),
            std::nullopt,
            "Unpack UI Prefab"),
        *m_registry);
    return true;
}

EntityID UIEditorPanel::ResolveGaugeRoot(EntityID entity) const
{
    if (!m_registry) {
        return Entity::NULL_ID;
    }

    EntityID current = entity;
    for (int guard = 0; guard < 64 && !Entity::IsNull(current) && m_registry->IsAlive(current); ++guard) {
        if (m_registry->GetComponent<HPGaugeBindingComponent>(current)) {
            return current;
        }
        current = GetParent(*m_registry, current);
    }
    return Entity::NULL_ID;
}

EntityID UIEditorPanel::FindSelectedGaugeRoot() const
{
    const EntityID fromSelection = ResolveGaugeRoot(m_selectedEntity);
    if (!Entity::IsNull(fromSelection)) {
        return fromSelection;
    }

    if (m_registry &&
        !Entity::IsNull(m_selectedGaugeRoot) &&
        m_registry->IsAlive(m_selectedGaugeRoot) &&
        m_registry->GetComponent<HPGaugeBindingComponent>(m_selectedGaugeRoot)) {
        return m_selectedGaugeRoot;
    }

    return Entity::NULL_ID;
}

EntityID UIEditorPanel::FindCanvas() const
{
    if (!m_registry) {
        return Entity::NULL_ID;
    }

    for (Archetype* archetype : m_registry->GetAllArchetypes()) {
        for (EntityID entity : archetype->GetEntities()) {
            if (!m_registry->IsAlive(entity)) {
                continue;
            }
            if (auto* name = m_registry->GetComponent<NameComponent>(entity)) {
                if (name->name == kCanvasName) {
                    return entity;
                }
            }
        }
    }
    return Entity::NULL_ID;
}

void UIEditorPanel::SelectEntity(EntityID entity)
{
    if (!m_registry || Entity::IsNull(entity) || !m_registry->IsAlive(entity)) {
        return;
    }

    m_selectedEntity = entity;
    const EntityID root = ResolveGaugeRoot(entity);
    if (!Entity::IsNull(root)) {
        m_selectedGaugeRoot = root;
    }
    EditorSelection::Instance().SelectEntity(entity);
}

void UIEditorPanel::ApplyPlacementPreset(EntityID entity, PlacementPreset preset)
{
    if (!m_registry || Entity::IsNull(entity) || !m_registry->IsAlive(entity)) {
        return;
    }

    auto* rect = m_registry->GetComponent<RectTransformComponent>(entity);
    if (!rect) {
        return;
    }

    const RectTransformComponent before = *rect;
    rect->anchoredPosition = PresetPosition(preset);
    switch (preset) {
    case PlacementPreset::TopLeft:
    case PlacementPreset::MiddleLeft:
    case PlacementPreset::BottomLeft:
        rect->pivot = { 0.0f, 0.5f };
        break;
    case PlacementPreset::TopRight:
    case PlacementPreset::MiddleRight:
    case PlacementPreset::BottomRight:
        rect->pivot = { 1.0f, 0.5f };
        break;
    default:
        rect->pivot = { 0.5f, 0.5f };
        break;
    }

    if (auto* transform = m_registry->GetComponent<TransformComponent>(entity)) {
        Editor2D::SyncRectTransformToTransform(*rect, *transform);
    }
    RecordComponentChange(*m_registry, entity, before, *rect);
}
