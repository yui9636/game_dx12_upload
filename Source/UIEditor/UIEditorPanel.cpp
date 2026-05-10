#include "UIEditor/UIEditorPanel.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iterator>
#include <limits>
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
#include "Font/FontManager.h"
#include "Gameplay/HealthComponent.h"
#include "Gameplay/HPGaugeComponent.h"
#include "Hierarchy/HierarchySystem.h"
#include "Icon/IconFontManager.h"
#include "Registry/Registry.h"
#include "System/PathResolver.h"
#include "System/UndoSystem.h"
#include "UIEditor/HPGaugeTemplateFactory.h"
#include "UIEditor/UIRectEvaluator.h"
#include "Undo/ComponentUndoAction.h"
#include "Undo/EntitySnapshot.h"
#include "Undo/EntityUndoActions.h"

namespace
{
    constexpr const char* kPrefabDirectory = "Data/UI/Prefabs";

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
    bool HasComponentChange(const T& before, const T& after)
    {
        return std::memcmp(&before, &after, sizeof(T)) != 0;
    }

    template <typename T>
    void RecordComponentChange(Registry& registry, EntityID entity, const T& before, const T& after)
    {
        UndoSystem::Instance().RecordAction(std::make_unique<ComponentUndoAction<T>>(entity, before, after));
        PrefabSystem::MarkPrefabOverride(entity, registry);
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

    std::vector<EntityID> CollectSubtree(Registry& registry, EntityID root)
    {
        std::vector<EntityID> entities;
        EntitySnapshot::CollectHierarchy(root, registry, entities);
        return entities;
    }

    bool InputTextString(const char* label, std::string& value)
    {
        char buffer[512] = {};
        std::snprintf(buffer, sizeof(buffer), "%s", value.c_str());
        bool changed = false;
        if (ImGui::InputText(label, buffer, sizeof(buffer))) {
            value = buffer;
            changed = true;
        }
        if (ImGui::BeginDragDropTarget()) {
            if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ENGINE_ASSET")) {
                value = static_cast<const char*>(payload->Data);
                changed = true;
            }
            ImGui::EndDragDropTarget();
        }
        return changed;
    }

    ImVec2 ToScreenPoint(const UIEditorViewState& view, const ImVec2& origin, float scale, const DirectX::XMFLOAT2& point)
    {
        return ImVec2(
            origin.x + view.referenceResolution.x * 0.5f * scale + (point.x + view.pan.x) * scale,
            origin.y + view.referenceResolution.y * 0.5f * scale - (point.y + view.pan.y) * scale);
    }

    std::string FormatPreviewHPText(const HPGaugeTextComponent& gaugeText, const UIEditorHPPreviewState& preview)
    {
        char buffer[128] = {};
        const int current = (std::max)(0, preview.currentHP);
        const int maxHP = (std::max)(0, preview.maxHP);
        switch (gaugeText.format) {
        case HPGaugeTextFormat::CurrentOnly:
            std::snprintf(buffer, sizeof(buffer), "%d", current);
            break;
        case HPGaugeTextFormat::Percent:
            std::snprintf(buffer, sizeof(buffer), "%d%%", static_cast<int>(std::round(preview.ratio * 100.0f)));
            break;
        case HPGaugeTextFormat::LabelCurrentMax:
            std::snprintf(buffer, sizeof(buffer), "%s %d / %d", gaugeText.label.c_str(), current, maxHP);
            break;
        case HPGaugeTextFormat::CurrentMax:
        default:
            std::snprintf(buffer, sizeof(buffer), "%d / %d", current, maxHP);
            break;
        }
        return buffer;
    }

    ImU32 ToColorU32(const DirectX::XMFLOAT4& color)
    {
        return ImGui::ColorConvertFloat4ToU32(ImVec4(color.x, color.y, color.z, color.w));
    }

    float ClampZoom(float zoom)
    {
        return (std::max)(0.10f, (std::min)(4.0f, zoom));
    }

    bool ContainsPoint(const ImVec2& min, const ImVec2& max, const ImVec2& point)
    {
        return point.x >= min.x && point.x <= max.x && point.y >= min.y && point.y <= max.y;
    }

    struct DesignerScreenRect
    {
        UIEditorRect entry;
        ImVec2 min;
        ImVec2 max;
        ImVec2 headerMin;
        ImVec2 headerMax;
        bool hasRootHeader = false;
    };

    std::vector<DesignerScreenRect> BuildDesignerScreenRects(Registry& registry,
                                                             const std::vector<UIEditorRect>& entries,
                                                             const UIEditorViewState& view,
                                                             const ImVec2& origin,
                                                             float scale)
    {
        std::vector<DesignerScreenRect> result;
        result.reserve(entries.size());
        for (const UIEditorRect& entry : entries) {
            const ImVec2 center = ToScreenPoint(view, origin, scale, entry.center);
            const ImVec2 size(entry.size.x * scale, entry.size.y * scale);

            DesignerScreenRect rect{};
            rect.entry = entry;
            rect.min = ImVec2(center.x - size.x * entry.pivot.x, center.y - size.y * (1.0f - entry.pivot.y));
            rect.max = ImVec2(rect.min.x + size.x, rect.min.y + size.y);
            rect.hasRootHeader = registry.GetComponent<HPGaugeBindingComponent>(entry.entity) != nullptr;
            if (rect.hasRootHeader) {
                const float headerHeight = (std::max)(18.0f, (std::min)(28.0f, rect.max.y - rect.min.y));
                rect.headerMin = rect.min;
                rect.headerMax = ImVec2(rect.max.x, rect.min.y + headerHeight);
            }
            result.push_back(rect);
        }
        return result;
    }

    const DesignerScreenRect* FindDesignerRect(const std::vector<DesignerScreenRect>& rects, EntityID entity)
    {
        for (const DesignerScreenRect& rect : rects) {
            if (rect.entry.entity == entity) {
                return &rect;
            }
        }
        return nullptr;
    }

    std::vector<EntityID> CollectHitCandidates(const std::vector<DesignerScreenRect>& rects, const ImVec2& mousePos)
    {
        std::vector<EntityID> result;
        auto appendUnique = [&](EntityID entity) {
            if (std::find(result.begin(), result.end(), entity) == result.end()) {
                result.push_back(entity);
            }
        };

        for (auto it = rects.rbegin(); it != rects.rend(); ++it) {
            if (it->hasRootHeader && ContainsPoint(it->headerMin, it->headerMax, mousePos)) {
                appendUnique(it->entry.entity);
            }
        }
        for (auto it = rects.rbegin(); it != rects.rend(); ++it) {
            if (ContainsPoint(it->min, it->max, mousePos)) {
                appendUnique(it->entry.entity);
            }
        }
        return result;
    }

    float ResolvePreviewFillRatio(const HPGaugeFillComponent& fill, const UIEditorHPPreviewState& preview)
    {
        if (!preview.enabled) {
            return 1.0f;
        }
        if (fill.useDelayedRatio) {
            return (std::max)(0.0f, (std::min)(1.0f, preview.delayedRatio));
        }
        return (std::max)(0.0f, (std::min)(1.0f, preview.ratio));
    }

    void ApplyFillRect(ImVec2& min, ImVec2& max, HPGaugeFillDirection direction, float ratio)
    {
        ratio = (std::max)(0.0f, (std::min)(1.0f, ratio));
        const float width = max.x - min.x;
        const float height = max.y - min.y;
        switch (direction) {
        case HPGaugeFillDirection::RightToLeft:
            min.x = max.x - width * ratio;
            break;
        case HPGaugeFillDirection::BottomToTop:
            min.y = max.y - height * ratio;
            break;
        case HPGaugeFillDirection::TopToBottom:
            max.y = min.y + height * ratio;
            break;
        case HPGaugeFillDirection::LeftToRight:
        default:
            max.x = min.x + width * ratio;
            break;
        }
    }

    void DrawTextPreview(ImDrawList* draw,
                         const UIEditorRect& entry,
                         const TextComponent& text,
                         const std::string& displayText,
                         const ImVec2& rectMin,
                         const ImVec2& rectMax)
    {
        if (displayText.empty()) {
            return;
        }

        ImFont* font = ImGui::GetFont();
        if (!text.fontAssetPath.empty()) {
            if (ImFont* previewFont = FontManager::Instance().GetEditorPreviewFont(text.fontAssetPath)) {
                font = previewFont;
            } else {
                FontManager::Instance().QueueEditorPreviewFont(text.fontAssetPath);
            }
        }

        const float fontSize = (std::max)(8.0f, text.fontSize);
        const float width = (std::max)(1.0f, rectMax.x - rectMin.x);
        const float height = (std::max)(1.0f, rectMax.y - rectMin.y);
        const float wrapWidth = text.wrapping ? width : 0.0f;
        const ImVec2 textSize = font->CalcTextSizeA(fontSize, (std::numeric_limits<float>::max)(), wrapWidth, displayText.c_str());

        ImVec2 textPos = rectMin;
        if (text.alignment == TextAlignment::Center) {
            textPos.x += (width - textSize.x) * 0.5f;
        } else if (text.alignment == TextAlignment::Right) {
            textPos.x += width - textSize.x;
        }
        textPos.y += (std::max)(0.0f, (height - textSize.y) * 0.5f);

        if (entry.canvas && entry.canvas->pixelSnap) {
            textPos.x = std::round(textPos.x);
            textPos.y = std::round(textPos.y);
        }

        draw->PushClipRect(rectMin, rectMax, true);
        draw->AddText(font, fontSize, textPos, ToColorU32(text.color), displayText.c_str(), nullptr, wrapWidth);
        draw->PopClipRect();
    }

}

void UIEditorPanel::DrawWorkspace(Registry* registry, bool* outFocused)
{
    m_registry = registry;

    ImGuiWindowFlags flags =
        ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoBringToFrontOnFocus;

    ImGui::Begin(ICON_FA_GAUGE_HIGH " UI Editor Workspace", nullptr, flags);
    if (outFocused) {
        *outFocused = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);
    }

    DrawToolbar();

    const float prefabBarHeight = 58.0f;
    const float mainHeight = (std::max)(1.0f, ImGui::GetContentRegionAvail().y - prefabBarHeight);
    ImGui::BeginChild("##UIEditorMain", ImVec2(0.0f, mainHeight), true, ImGuiWindowFlags_NoScrollbar);

    const float paletteWidth = 240.0f;
    const float propertiesWidth = 330.0f;
    const float spacing = ImGui::GetStyle().ItemSpacing.x;

    ImGui::BeginChild("##UIEditorPalette", ImVec2(paletteWidth, 0.0f), true);
    DrawPalette();
    ImGui::EndChild();

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
    m_viewState.zoom = ClampZoom(m_viewState.zoom);
    ImGui::TextUnformatted(ICON_FA_LAYER_GROUP " UI Editor");
    ImGui::SameLine();
    ImGui::TextDisabled("|");
    ImGui::SameLine();
    ImGui::Text("Authoring Canvas: %s", (m_registry ? GetName(*m_registry, canvas, "Not created").c_str() : "No Registry"));
    ImGui::SameLine();
    ImGui::TextDisabled("Prefab authoring only. Place saved prefabs in Level Editor.");
    ImGui::SameLine();
    ImGui::Checkbox("Snap", &m_viewState.snapToGrid);
    ImGui::SameLine();
    ImGui::Checkbox("Safe Area", &m_viewState.showSafeArea);
    ImGui::SameLine();
    ImGui::Checkbox("Grid", &m_viewState.showGrid);
    ImGui::SameLine();
    ImGui::Text("Zoom: %.0f%%", m_viewState.zoom * 100.0f);
    ImGui::SameLine();
    if (ImGui::SmallButton("100%")) {
        m_viewState.zoom = 1.0f;
        m_viewState.pan = { 0.0f, 0.0f };
        SetStatusMessage("Designer view reset to full canvas.", true, 2.5f);
    }
    ImGui::SameLine();
    const std::array<float, 5> gridSizes = { 10.0f, 20.0f, 40.0f, 80.0f, 120.0f };
    ImGui::SetNextItemWidth(84.0f);
    if (ImGui::BeginCombo("Grid", (std::to_string(static_cast<int>(m_viewState.gridSize)) + "px").c_str())) {
        for (float gridSize : gridSizes) {
            const bool selected = std::fabs(m_viewState.gridSize - gridSize) < 0.001f;
            if (ImGui::Selectable((std::to_string(static_cast<int>(gridSize)) + "px").c_str(), selected)) {
                m_viewState.gridSize = gridSize;
            }
            if (selected) {
                ImGui::SetItemDefaultFocus();
            }
        }
        ImGui::EndCombo();
    }
    ImGui::Separator();
    DrawStatusLine();
}

void UIEditorPanel::DrawStatusLine()
{
    if (m_interactionState.statusTimer > 0.0f) {
        m_interactionState.statusTimer = (std::max)(0.0f, m_interactionState.statusTimer - ImGui::GetIO().DeltaTime);
    }

    const EntityID root = FindSelectedGaugeRoot();
    ImGui::Text("Selected: %s", (m_registry ? GetName(*m_registry, m_selectedEntity, "None").c_str() : "None"));
    ImGui::SameLine();
    ImGui::TextDisabled("|");
    ImGui::SameLine();
    ImGui::Text("Gauge Root: %s", (m_registry ? GetName(*m_registry, root, "None").c_str() : "None"));

    if (!m_interactionState.statusMessage.empty() && m_interactionState.statusTimer > 0.0f) {
        ImGui::SameLine();
        ImGui::TextDisabled("|");
        ImGui::SameLine();
        const ImVec4 color = m_interactionState.statusSuccess ? ImVec4(0.45f, 0.88f, 0.55f, 1.0f) : ImVec4(1.0f, 0.45f, 0.38f, 1.0f);
        ImGui::TextColored(color, "%s", m_interactionState.statusMessage.c_str());
    }
}

void UIEditorPanel::DrawPalette()
{
    ImGui::TextUnformatted("Templates");
    if (ImGui::Button("Template...", ImVec2(-1.0f, 0.0f))) {
        ImGui::OpenPopup("UIEditorTemplatePopup");
    }
    if (ImGui::BeginPopup("UIEditorTemplatePopup")) {
        if (ImGui::MenuItem("Player HP")) {
            CreateTemplate(UIEditorTemplateKind::PlayerHP);
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("%s", UIEditorTemplates::GetTemplatePlacementHint(UIEditorTemplateKind::PlayerHP));
        }
        if (ImGui::MenuItem("Boss HP")) {
            CreateTemplate(UIEditorTemplateKind::BossHP);
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("%s", UIEditorTemplates::GetTemplatePlacementHint(UIEditorTemplateKind::BossHP));
        }
        ImGui::EndPopup();
    }

    ImGui::Spacing();
    ImGui::SeparatorText("Parts");
    const std::array<UIEditorPartKind, 6> parts = {
        UIEditorPartKind::Canvas,
        UIEditorPartKind::GaugeRoot,
        UIEditorPartKind::Image,
        UIEditorPartKind::FillImage,
        UIEditorPartKind::DamagePreview,
        UIEditorPartKind::HPText
    };
    for (UIEditorPartKind part : parts) {
        if (ImGui::Button(UIEditorTemplates::GetPartName(part), ImVec2(-1.0f, 0.0f))) {
            CreatePart(part);
        }
    }

    ImGui::Spacing();
    ImGui::SeparatorText("Saved Prefabs");
    ImGui::TextWrapped("Saved UI prefabs are placed from the Level Editor / SceneView.");
    const std::vector<std::filesystem::path> prefabs = CollectPrefabs();
    if (prefabs.empty()) {
        ImGui::TextDisabled("No prefabs in Data/UI/Prefabs.");
    } else {
        for (const std::filesystem::path& path : prefabs) {
            if (ImGui::Selectable(path.filename().string().c_str(), m_prefabState.lastPrefabPath == path)) {
                m_prefabState.lastPrefabPath = path;
            }
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("Use Asset Browser or Level Editor to place:\n%s", path.string().c_str());
            }
        }
    }
}

void UIEditorPanel::DrawDesignerView()
{
    const float treeHeight = 170.0f;
    const ImVec2 avail = ImGui::GetContentRegionAvail();
    ImGui::BeginChild("##UIEditorDesigner", ImVec2(0.0f, (std::max)(180.0f, avail.y - treeHeight)), true, ImGuiWindowFlags_NoScrollbar);
    ImGui::TextUnformatted("Designer View");
    ImGui::SameLine();
    ImGui::TextDisabled("Wheel: zoom | Middle/Space drag: pan | F: frame | Home: canvas | Alt-click: cycle overlap");

    const ImVec2 canvasAvail = ImGui::GetContentRegionAvail();
    const float fitScale = (std::min)(canvasAvail.x / m_viewState.referenceResolution.x, (canvasAvail.y - 8.0f) / m_viewState.referenceResolution.y);
    m_viewState.zoom = ClampZoom(m_viewState.zoom);
    float scale = (std::max)(0.05f, fitScale * m_viewState.zoom);
    ImVec2 canvasSize(m_viewState.referenceResolution.x * scale, m_viewState.referenceResolution.y * scale);
    const ImVec2 cursor = ImGui::GetCursorScreenPos();
    ImVec2 origin(
        cursor.x + (canvasAvail.x - canvasSize.x) * 0.5f,
        cursor.y + (canvasAvail.y - canvasSize.y) * 0.5f);
    ImVec2 canvasMax(origin.x + canvasSize.x, origin.y + canvasSize.y);

    ImGuiIO& io = ImGui::GetIO();
    const bool designerHovered = ImGui::IsWindowHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem);
    if (designerHovered && ContainsPoint(origin, canvasMax, io.MousePos) && std::fabs(io.MouseWheel) > 0.0001f) {
        const ImVec2 viewCenter(origin.x + canvasSize.x * 0.5f, origin.y + canvasSize.y * 0.5f);
        const DirectX::XMFLOAT2 pointUnderCursor = {
            (io.MousePos.x - viewCenter.x) / scale - m_viewState.pan.x,
            -(io.MousePos.y - viewCenter.y) / scale - m_viewState.pan.y
        };
        const float zoomFactor = std::pow(1.12f, io.MouseWheel);
        m_viewState.zoom = ClampZoom(m_viewState.zoom * zoomFactor);
        scale = (std::max)(0.05f, fitScale * m_viewState.zoom);
        canvasSize = ImVec2(m_viewState.referenceResolution.x * scale, m_viewState.referenceResolution.y * scale);
        origin = ImVec2(
            cursor.x + (canvasAvail.x - canvasSize.x) * 0.5f,
            cursor.y + (canvasAvail.y - canvasSize.y) * 0.5f);
        canvasMax = ImVec2(origin.x + canvasSize.x, origin.y + canvasSize.y);
        const ImVec2 newViewCenter(origin.x + canvasSize.x * 0.5f, origin.y + canvasSize.y * 0.5f);
        m_viewState.pan.x = (io.MousePos.x - newViewCenter.x) / scale - pointUnderCursor.x;
        m_viewState.pan.y = -(io.MousePos.y - newViewCenter.y) / scale - pointUnderCursor.y;
    }

    ImDrawList* draw = ImGui::GetWindowDrawList();
    draw->AddRectFilled(origin, canvasMax, IM_COL32(22, 24, 29, 255));
    draw->AddRect(origin, canvasMax, IM_COL32(90, 96, 112, 255));

    if (m_viewState.showSafeArea) {
        const float marginX = 64.0f * scale;
        const float marginY = 36.0f * scale;
        draw->AddRect(
            ImVec2(origin.x + marginX, origin.y + marginY),
            ImVec2(origin.x + canvasSize.x - marginX, origin.y + canvasSize.y - marginY),
            IM_COL32(80, 170, 255, 120));
    }

    if (m_viewState.showGrid) {
        const float grid = (std::max)(8.0f, m_viewState.gridSize);
        for (float x = grid; x < m_viewState.referenceResolution.x; x += grid) {
            const float sx = origin.x + x * scale;
            draw->AddLine(ImVec2(sx, origin.y), ImVec2(sx, canvasMax.y), IM_COL32(255, 255, 255, 18));
        }
        for (float y = grid; y < m_viewState.referenceResolution.y; y += grid) {
            const float sy = origin.y + y * scale;
            draw->AddLine(ImVec2(origin.x, sy), ImVec2(canvasMax.x, sy), IM_COL32(255, 255, 255, 18));
        }
    }

    std::vector<DesignerScreenRect> screenRects;
    if (m_registry) {
        HierarchySystem hierarchySystem;
        hierarchySystem.Update(*m_registry);

        const EntityID canvas = FindCanvas();
        if (!Entity::IsNull(canvas)) {
            const std::vector<UIEditorRect> entries = UIEditor::UIRectEvaluator::CollectCanvasWidgets(*m_registry, canvas);
            screenRects = BuildDesignerScreenRects(*m_registry, entries, m_viewState, origin, scale);

            ImGui::SetCursorScreenPos(origin);
            ImGui::InvisibleButton("##UIEditorDesignerSurface", canvasSize, ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonMiddle);
            const bool surfaceHovered = ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem);
            const bool inCanvas = surfaceHovered && ContainsPoint(origin, canvasMax, io.MousePos);

            m_interactionState.hoveredEntity = Entity::NULL_ID;
            std::vector<EntityID> candidates;
            if (inCanvas) {
                candidates = CollectHitCandidates(screenRects, io.MousePos);
                if (!candidates.empty()) {
                    m_interactionState.hoveredEntity = candidates.front();
                }
            }

            const DesignerScreenRect* selectedRect = FindDesignerRect(screenRects, m_selectedEntity);
            bool resizeHandleHovered = false;
            if (selectedRect) {
                const ImVec2 handleSize(14.0f, 14.0f);
                const ImVec2 handleMin(selectedRect->max.x - handleSize.x, selectedRect->max.y - handleSize.y);
                resizeHandleHovered = inCanvas && ContainsPoint(handleMin, selectedRect->max, io.MousePos);
                if (resizeHandleHovered) {
                    m_interactionState.hoveredEntity = m_selectedEntity;
                }
            }

            if (inCanvas && !io.WantTextInput) {
                if (ImGui::IsKeyPressed(ImGuiKey_Home, false)) {
                    m_viewState.pan = { 0.0f, 0.0f };
                    m_viewState.zoom = 1.0f;
                    SetStatusMessage("Framed authoring canvas.", true, 2.0f);
                } else if (!io.KeyCtrl && !io.KeyAlt && ImGui::IsKeyPressed(ImGuiKey_F, false) && selectedRect) {
                    const float selectedWidth = (std::max)(1.0f, selectedRect->entry.size.x);
                    const float selectedHeight = (std::max)(1.0f, selectedRect->entry.size.y);
                    const float desiredPixels = (std::max)(180.0f, (std::min)(canvasAvail.x, canvasAvail.y) * 0.45f);
                    const float fitPixels = (std::max)(selectedWidth, selectedHeight) * fitScale;
                    m_viewState.zoom = ClampZoom((std::max)(1.0f, desiredPixels / (std::max)(1.0f, fitPixels)));
                    m_viewState.pan = { -selectedRect->entry.center.x, -selectedRect->entry.center.y };
                    SetStatusMessage("Framed selected widget.", true, 2.0f);
                }
            }

            if (inCanvas && (resizeHandleHovered || !candidates.empty())) {
                ImGui::SetMouseCursor(resizeHandleHovered ? ImGuiMouseCursor_ResizeNWSE : ImGuiMouseCursor_ResizeAll);
            }

            const bool spaceDown = ImGui::IsKeyDown(ImGuiKey_Space);
            if (inCanvas && !spaceDown && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
                if (resizeHandleHovered && selectedRect) {
                    m_interactionState.draggingRect = false;
                    m_interactionState.resizingRect = true;
                    m_interactionState.editEntity = m_selectedEntity;
                    if (auto* rect = m_registry->GetComponent<RectTransformComponent>(m_selectedEntity)) {
                        m_interactionState.beforeRect = *rect;
                    }
                } else if (!candidates.empty()) {
                    EntityID chosen = candidates.front();
                    if (io.KeyAlt && candidates.size() > 1) {
                        auto currentIt = std::find(candidates.begin(), candidates.end(), m_selectedEntity);
                        if (currentIt != candidates.end()) {
                            ++currentIt;
                            chosen = (currentIt == candidates.end()) ? candidates.front() : *currentIt;
                        }
                        SelectEntity(chosen);
                    } else if (candidates.size() > 1) {
                        m_interactionState.pickCandidates = candidates;
                        m_interactionState.pickPopupScreenPos = { io.MousePos.x, io.MousePos.y };
                        ImGui::OpenPopup("UIEditorCandidatePicker");
                        chosen = Entity::NULL_ID;
                    } else {
                        SelectEntity(chosen);
                    }

                    if (!Entity::IsNull(chosen)) {
                        m_interactionState.draggingRect = true;
                        m_interactionState.resizingRect = false;
                        m_interactionState.editEntity = chosen;
                        if (auto* rect = m_registry->GetComponent<RectTransformComponent>(chosen)) {
                            m_interactionState.beforeRect = *rect;
                        }
                    }
                }
            }

            if (surfaceHovered && ImGui::IsMouseClicked(ImGuiMouseButton_Middle)) {
                m_interactionState.panningView = true;
            }
            if (surfaceHovered && spaceDown && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
                m_interactionState.panningView = true;
            }
            if (m_interactionState.panningView) {
                if (ImGui::IsMouseDown(ImGuiMouseButton_Middle) || (spaceDown && ImGui::IsMouseDown(ImGuiMouseButton_Left))) {
                    m_viewState.pan.x += io.MouseDelta.x / scale;
                    m_viewState.pan.y -= io.MouseDelta.y / scale;
                    ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeAll);
                } else {
                    m_interactionState.panningView = false;
                }
            }

            if ((m_interactionState.draggingRect || m_interactionState.resizingRect) &&
                ImGui::IsMouseDown(ImGuiMouseButton_Left) &&
                !Entity::IsNull(m_interactionState.editEntity) &&
                m_registry->IsAlive(m_interactionState.editEntity)) {
                if (auto* rect = m_registry->GetComponent<RectTransformComponent>(m_interactionState.editEntity)) {
                    const ImVec2 delta = io.MouseDelta;
                    const float step = io.KeyCtrl ? 1.0f : (io.KeyShift ? 10.0f : (std::max)(1.0f, m_viewState.gridSize));
                    const bool snap = (m_viewState.snapToGrid || io.KeyCtrl || io.KeyShift) && !io.KeyAlt;
                    if (m_interactionState.draggingRect) {
                        rect->anchoredPosition.x += delta.x / scale;
                        rect->anchoredPosition.y -= delta.y / scale;
                        if (snap) {
                            rect->anchoredPosition.x = std::round(rect->anchoredPosition.x / step) * step;
                            rect->anchoredPosition.y = std::round(rect->anchoredPosition.y / step) * step;
                        }
                    } else {
                        rect->sizeDelta.x = (std::max)(1.0f, rect->sizeDelta.x + delta.x / scale);
                        rect->sizeDelta.y = (std::max)(1.0f, rect->sizeDelta.y + delta.y / scale);
                        if (snap) {
                            rect->sizeDelta.x = (std::max)(1.0f, std::round(rect->sizeDelta.x / step) * step);
                            rect->sizeDelta.y = (std::max)(1.0f, std::round(rect->sizeDelta.y / step) * step);
                        }
                    }
                    SyncRectEdit(m_interactionState.editEntity);
                    const std::vector<UIEditorRect> updatedEntries = UIEditor::UIRectEvaluator::CollectCanvasWidgets(*m_registry, canvas);
                    screenRects = BuildDesignerScreenRects(*m_registry, updatedEntries, m_viewState, origin, scale);
                }
            }

            for (const DesignerScreenRect& screenRect : screenRects) {
                const UIEditorRect& entry = screenRect.entry;
                const bool selected = entry.entity == m_selectedEntity || entry.entity == m_selectedGaugeRoot;

                ImU32 fillColor = IM_COL32(110, 160, 255, selected ? 105 : 55);
                if (entry.sprite) {
                    fillColor = ToColorU32(entry.sprite->tint);
                }
                if (m_registry->GetComponent<HPGaugeBindingComponent>(entry.entity)) {
                    fillColor = IM_COL32(80, 180, 100, selected ? 120 : 70);
                }
                if (auto* fill = m_registry->GetComponent<HPGaugeFillComponent>(entry.entity)) {
                    ImVec2 fillMin = screenRect.min;
                    ImVec2 fillMax = screenRect.max;
                    ApplyFillRect(fillMin, fillMax, fill->fillDirection, ResolvePreviewFillRatio(*fill, m_previewState));
                    draw->AddRectFilled(fillMin, fillMax, fillColor);
                    draw->AddRect(screenRect.min, screenRect.max, IM_COL32(160, 170, 190, 90), 0.0f, 0, 1.0f);
                } else {
                    draw->AddRectFilled(screenRect.min, screenRect.max, fillColor);
                }

                if (screenRect.hasRootHeader) {
                    draw->AddRectFilled(screenRect.headerMin, screenRect.headerMax, IM_COL32(28, 42, 34, 230));
                    draw->AddLine(ImVec2(screenRect.headerMin.x, screenRect.headerMax.y), screenRect.headerMax, IM_COL32(115, 230, 145, 180), 2.0f);
                    const std::string label = "[Gauge Root] " + GetName(*m_registry, entry.entity);
                    draw->AddText(ImVec2(screenRect.headerMin.x + 6.0f, screenRect.headerMin.y + 3.0f), IM_COL32(205, 255, 215, 255), label.c_str());
                }

                if (entry.text) {
                    std::string displayText = entry.text->text;
                    if (auto* gaugeText = m_registry->GetComponent<HPGaugeTextComponent>(entry.entity)) {
                        displayText = FormatPreviewHPText(*gaugeText, m_previewState);
                    }
                    DrawTextPreview(draw, entry, *entry.text, displayText, screenRect.min, screenRect.max);
                }

                draw->AddRect(screenRect.min, screenRect.max, IM_COL32(170, 190, 220, 120), 0.0f, 0, 1.0f);
            }

            for (const DesignerScreenRect& screenRect : screenRects) {
                const bool hovered = screenRect.entry.entity == m_interactionState.hoveredEntity;
                const bool selected = screenRect.entry.entity == m_selectedEntity || screenRect.entry.entity == m_selectedGaugeRoot;
                if (!hovered && !selected) {
                    continue;
                }

                const ImU32 outlineColor = selected ? IM_COL32(255, 220, 90, 255) : IM_COL32(90, 180, 255, 230);
                draw->AddRect(screenRect.min, screenRect.max, outlineColor, 0.0f, 0, selected ? 2.5f : 2.0f);

                const std::string label = GetName(*m_registry, screenRect.entry.entity);
                const ImVec2 textSize = ImGui::CalcTextSize(label.c_str());
                const ImVec2 labelMin(screenRect.min.x, screenRect.min.y - textSize.y - 6.0f);
                const ImVec2 labelMax(labelMin.x + textSize.x + 10.0f, labelMin.y + textSize.y + 4.0f);
                draw->AddRectFilled(labelMin, labelMax, selected ? IM_COL32(75, 58, 20, 235) : IM_COL32(24, 45, 70, 235), 3.0f);
                draw->AddText(ImVec2(labelMin.x + 5.0f, labelMin.y + 2.0f), IM_COL32(255, 255, 255, 255), label.c_str());

                if (selected && screenRect.entry.entity == m_selectedEntity) {
                    const ImVec2 handleSize(14.0f, 14.0f);
                    const ImVec2 handleMin(screenRect.max.x - handleSize.x, screenRect.max.y - handleSize.y);
                    draw->AddRectFilled(handleMin, screenRect.max, IM_COL32(255, 220, 90, 245), 2.0f);
                    draw->AddRect(handleMin, screenRect.max, IM_COL32(20, 20, 20, 230), 2.0f, 0, 1.0f);
                }
            }

            if (m_interactionState.draggingRect || m_interactionState.resizingRect) {
                if (const DesignerScreenRect* activeRect = FindDesignerRect(screenRects, m_interactionState.editEntity)) {
                    draw->AddRect(activeRect->min, activeRect->max, IM_COL32(255, 255, 255, 150), 0.0f, 0, 3.0f);
                }
            }

            if (!m_interactionState.pickCandidates.empty()) {
                ImGui::SetNextWindowPos(ImVec2(m_interactionState.pickPopupScreenPos.x, m_interactionState.pickPopupScreenPos.y), ImGuiCond_Appearing);
                if (ImGui::BeginPopup("UIEditorCandidatePicker")) {
                    ImGui::TextUnformatted("Select widget");
                    ImGui::Separator();
                    for (EntityID entity : m_interactionState.pickCandidates) {
                        if (!m_registry->IsAlive(entity)) {
                            continue;
                        }
                        const std::string label = GetName(*m_registry, entity);
                        if (ImGui::Selectable(label.c_str(), entity == m_selectedEntity)) {
                            SelectEntity(entity);
                            m_interactionState.pickCandidates.clear();
                            ImGui::CloseCurrentPopup();
                        }
                    }
                    ImGui::EndPopup();
                } else if (!ImGui::IsPopupOpen("UIEditorCandidatePicker")) {
                    m_interactionState.pickCandidates.clear();
                }
            }
        } else {
            const char* text = "Create a Canvas, then add a Template or Parts.";
            const ImVec2 textSize = ImGui::CalcTextSize(text);
            draw->AddText(ImVec2(origin.x + (canvasSize.x - textSize.x) * 0.5f, origin.y + (canvasSize.y - textSize.y) * 0.5f), IM_COL32(190, 195, 205, 255), text);
        }
    }

    CommitDesignerRectEdit();
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
        ImGui::TextDisabled("No authoring canvas.");
        ImGui::EndChild();
        return;
    }

    std::function<void(EntityID)> drawNode = [&](EntityID entity) {
        if (!m_registry->IsAlive(entity)) {
            return;
        }

        auto* hierarchy = m_registry->GetComponent<HierarchyComponent>(entity);
        const bool hasChildren = hierarchy && !Entity::IsNull(hierarchy->firstChild);
        ImGuiTreeNodeFlags flags =
            ImGuiTreeNodeFlags_OpenOnArrow |
            ImGuiTreeNodeFlags_SpanAvailWidth |
            (hasChildren ? 0 : ImGuiTreeNodeFlags_Leaf) |
            ((entity == m_selectedEntity) ? ImGuiTreeNodeFlags_Selected : 0);

        std::string label = GetName(*m_registry, entity);
        if (m_registry->GetComponent<HPGaugeBindingComponent>(entity)) {
            label = "[Gauge] " + label;
        } else if (m_registry->GetComponent<HPGaugeFillComponent>(entity)) {
            label = "[Fill] " + label;
        } else if (m_registry->GetComponent<TextComponent>(entity)) {
            label = "[Text] " + label;
        } else if (m_registry->GetComponent<SpriteComponent>(entity)) {
            label = "[Image] " + label;
        }

        const bool open = ImGui::TreeNodeEx(reinterpret_cast<void*>(static_cast<uintptr_t>(entity)), flags, "%s", label.c_str());
        if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen()) {
            SelectEntity(entity);
        }

        if (open) {
            EntityID child = hierarchy ? hierarchy->firstChild : Entity::NULL_ID;
            while (!Entity::IsNull(child) && m_registry->IsAlive(child)) {
                EntityID next = Entity::NULL_ID;
                if (auto* childHierarchy = m_registry->GetComponent<HierarchyComponent>(child)) {
                    next = childHierarchy->nextSibling;
                }
                drawNode(child);
                child = next;
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

    const EntityID gaugeRoot = FindSelectedGaugeRoot();
    ImGui::SeparatorText("Selection Summary");
    ImGui::Text("Entity: %s", GetName(*m_registry, m_selectedEntity).c_str());
    ImGui::Text("Parent: %s", GetName(*m_registry, GetParent(*m_registry, m_selectedEntity), "None").c_str());
    ImGui::Text("HP Gauge Root: %s", GetName(*m_registry, gaugeRoot, "None").c_str());
    if (!Entity::IsNull(gaugeRoot) && m_registry->IsAlive(gaugeRoot)) {
        if (auto* prefab = m_registry->GetComponent<PrefabInstanceComponent>(gaugeRoot)) {
            ImGui::Text("Prefab: %s%s", prefab->prefabAssetPath.c_str(), prefab->hasOverrides ? " (override)" : "");
        } else {
            ImGui::TextDisabled("Prefab: None");
        }
    } else {
        ImGui::TextDisabled("Prefab: None");
    }
    DrawPreviewHPPanel(gaugeRoot);

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
        ImGui::SeparatorText("Rect Transform");
        RectTransformComponent before = *rect;
        bool changed = false;
        changed |= ImGui::DragFloat2("Position", &rect->anchoredPosition.x, 1.0f);
        changed |= ImGui::DragFloat2("Size", &rect->sizeDelta.x, 1.0f, 1.0f, 4096.0f);
        changed |= ImGui::DragFloat2("Anchor Min", &rect->anchorMin.x, 0.01f, 0.0f, 1.0f);
        changed |= ImGui::DragFloat2("Anchor Max", &rect->anchorMax.x, 0.01f, 0.0f, 1.0f);
        changed |= ImGui::DragFloat2("Pivot", &rect->pivot.x, 0.01f, 0.0f, 1.0f);
        changed |= ImGui::DragFloat("Rotation Z", &rect->rotationZ, 0.25f, -360.0f, 360.0f);
        changed |= ImGui::DragFloat2("Scale", &rect->scale2D.x, 0.01f, 0.001f, 100.0f);
        if (changed) {
            SyncRectEdit(m_selectedEntity);
            RecordComponentChange(*m_registry, m_selectedEntity, before, *rect);
        }
    }

    if (auto* canvas = m_registry->GetComponent<CanvasItemComponent>(m_selectedEntity)) {
        ImGui::SeparatorText("Canvas Item");
        CanvasItemComponent before = *canvas;
        bool changed = false;
        changed |= ImGui::DragInt("Sorting Layer", &canvas->sortingLayer, 1);
        changed |= ImGui::DragInt("Order In Layer", &canvas->orderInLayer, 1);
        changed |= ImGui::Checkbox("Visible", &canvas->visible);
        changed |= ImGui::Checkbox("Interactable", &canvas->interactable);
        changed |= ImGui::Checkbox("Pixel Snap", &canvas->pixelSnap);
        changed |= ImGui::Checkbox("Lock Aspect", &canvas->lockAspect);
        if (changed) {
            RecordComponentChange(*m_registry, m_selectedEntity, before, *canvas);
        }
    }

    if (auto* binding = m_registry->GetComponent<HPGaugeBindingComponent>(m_selectedEntity)) {
        ImGui::SeparatorText("HP Gauge Binding");
        const char* modes[] = { "Explicit", "First Player", "First Enemy", "First Boss" };
        int mode = static_cast<int>(binding->targetMode);
        if (ImGui::Combo("Target Mode", &mode, modes, IM_ARRAYSIZE(modes))) {
            const HPGaugeBindingComponent before = *binding;
            binding->targetMode = static_cast<HPGaugeTargetMode>(mode);
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

    }

    if (auto* sprite = m_registry->GetComponent<SpriteComponent>(m_selectedEntity)) {
        ImGui::SeparatorText("Image");
        SpriteComponent before = *sprite;
        bool changed = false;
        changed |= InputTextString("Texture", sprite->textureAssetPath);
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
        changed |= ImGui::Checkbox("Use Displayed Ratio", &fill->useDisplayedRatio);
        changed |= ImGui::Checkbox("Use Delayed Ratio", &fill->useDelayedRatio);
        changed |= ImGui::Checkbox("Hide When No Target", &fill->hideWhenNoTarget);
        changed |= ImGui::Checkbox("Hide When Full", &fill->hideWhenFull);
        changed |= ImGui::DragFloat("Min Visible Ratio", &fill->minVisibleRatio, 0.01f, 0.0f, 1.0f);
        changed |= ImGui::ColorEdit4("Fixed", &fill->fixedColor.x);
        changed |= ImGui::ColorEdit4("High", &fill->highColor.x);
        changed |= ImGui::ColorEdit4("Mid", &fill->midColor.x);
        changed |= ImGui::ColorEdit4("Low", &fill->lowColor.x);
        changed |= ImGui::DragFloat("Mid Threshold", &fill->midThreshold, 0.01f, 0.0f, 1.0f);
        changed |= ImGui::DragFloat("Low Threshold", &fill->lowThreshold, 0.01f, 0.0f, 1.0f);
        if (changed) {
            RecordComponentChange(*m_registry, m_selectedEntity, before, *fill);
        }
    }

    if (auto* text = m_registry->GetComponent<TextComponent>(m_selectedEntity)) {
        ImGui::SeparatorText("Text");
        TextComponent before = *text;
        bool changed = false;
        changed |= InputTextString("Text", text->text);
        changed |= InputTextString("Font", text->fontAssetPath);
        changed |= ImGui::DragFloat("Font Size", &text->fontSize, 0.5f, 4.0f, 128.0f);
        changed |= ImGui::ColorEdit4("Color", &text->color.x);
        const char* alignments[] = { "Left", "Center", "Right" };
        int alignment = static_cast<int>(text->alignment);
        if (ImGui::Combo("Alignment", &alignment, alignments, IM_ARRAYSIZE(alignments))) {
            text->alignment = static_cast<TextAlignment>(alignment);
            changed = true;
        }
        changed |= ImGui::DragFloat("Line Spacing", &text->lineSpacing, 0.01f, 0.1f, 4.0f);
        changed |= ImGui::Checkbox("Wrapping", &text->wrapping);
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

        HPGaugeTextComponent before = *gaugeText;
        bool changed = false;
        changed |= InputTextString("Label", gaugeText->label);
        changed |= ImGui::Checkbox("Hide When No Target", &gaugeText->hideWhenNoTarget);
        changed |= ImGui::Checkbox("Hide When Dead", &gaugeText->hideWhenDead);
        changed |= ImGui::Checkbox("Hide When Full", &gaugeText->hideWhenFull);
        if (changed) {
            RecordComponentChange(*m_registry, m_selectedEntity, before, *gaugeText);
        }
    }
}

void UIEditorPanel::DrawPreviewHPPanel(EntityID gaugeRoot)
{
    if (!m_registry || Entity::IsNull(gaugeRoot) || !m_registry->IsAlive(gaugeRoot)) {
        return;
    }

    ImGui::SeparatorText("Preview HP");
    ImGui::Checkbox("Enable Preview", &m_previewState.enabled);

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
            m_previewState.ratio = ratios[i].second;
            m_previewState.maxHP = (std::max)(1, m_previewState.maxHP);
            m_previewState.currentHP = static_cast<int>(std::round(m_previewState.ratio * static_cast<float>(m_previewState.maxHP)));
            m_previewState.delayedRatio = (std::max)(m_previewState.delayedRatio, m_previewState.ratio);
        }
    }

    if (ImGui::SliderFloat("Ratio", &m_previewState.ratio, 0.0f, 1.0f, "%.2f")) {
        m_previewState.maxHP = (std::max)(1, m_previewState.maxHP);
        m_previewState.currentHP = static_cast<int>(std::round(m_previewState.ratio * static_cast<float>(m_previewState.maxHP)));
    }
    if (ImGui::InputInt("Current HP", &m_previewState.currentHP)) {
        m_previewState.maxHP = (std::max)(1, m_previewState.maxHP);
        m_previewState.currentHP = (std::max)(0, (std::min)(m_previewState.currentHP, m_previewState.maxHP));
        m_previewState.ratio = static_cast<float>(m_previewState.currentHP) / static_cast<float>(m_previewState.maxHP);
    }
    if (ImGui::InputInt("Max HP", &m_previewState.maxHP)) {
        m_previewState.maxHP = (std::max)(1, m_previewState.maxHP);
        m_previewState.currentHP = (std::max)(0, (std::min)(m_previewState.currentHP, m_previewState.maxHP));
        m_previewState.ratio = static_cast<float>(m_previewState.currentHP) / static_cast<float>(m_previewState.maxHP);
    }
    ImGui::SliderFloat("Delayed Ratio", &m_previewState.delayedRatio, 0.0f, 1.0f, "%.2f");
    if (ImGui::Button("Play Damage Preview", ImVec2(-1.0f, 0.0f))) {
        m_previewState.delayedRatio = 1.0f;
        m_previewState.ratio = 0.5f;
        m_previewState.maxHP = 100;
        m_previewState.currentHP = 50;
        SetStatusMessage("Preview damage state applied. Runtime HP data is untouched.", true, 2.5f);
    }
}

void UIEditorPanel::DrawPrefabBar()
{
    const EntityID root = FindSelectedGaugeRoot();
    const bool hasRoot = !Entity::IsNull(root);
    ImGui::Text("Prefab Target: %s", hasRoot ? GetName(*m_registry, root).c_str() : "None");
    ImGui::SameLine();
    ImGui::BeginDisabled(!hasRoot);
    if (ImGui::Button("Save HP Gauge Prefab")) {
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
    ImGui::SameLine();
    ImGui::TextDisabled("Place saved prefabs from Level Editor / SceneView.");

    if (!m_prefabState.lastPrefabPath.empty()) {
        ImGui::TextDisabled("%s", m_prefabState.lastPrefabPath.string().c_str());
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

    return ExecuteCreateSnapshot(*m_registry, UIEditorTemplates::BuildCanvasSnapshot(m_viewState.referenceResolution), Entity::NULL_ID, "Create UI Authoring Canvas");
}

EntityID UIEditorPanel::CreateTemplate(UIEditorTemplateKind kind)
{
    if (!m_registry) {
        return Entity::NULL_ID;
    }

    const EntityID canvas = FindOrCreateCanvas();
    if (Entity::IsNull(canvas)) {
        return Entity::NULL_ID;
    }

    EntityID root = ExecuteCreateSnapshot(*m_registry, UIEditorTemplates::BuildTemplateSnapshot(kind), canvas, "Create HP Gauge Template");
    if (!Entity::IsNull(root)) {
        SelectEntity(root);
        HierarchySystem hierarchySystem;
        hierarchySystem.Update(*m_registry);
        UIEditorRect rect;
        if (UIEditor::UIRectEvaluator::Evaluate(*m_registry, root, rect)) {
            m_viewState.pan = { -rect.center.x, -rect.center.y };
            m_viewState.zoom = (std::max)(1.0f, m_viewState.zoom);
        }
        SetStatusMessage(std::string(UIEditorTemplates::GetTemplateName(kind)) + " template created. Edit parts, then save as prefab.");
    }
    return root;
}

EntityID UIEditorPanel::CreatePart(UIEditorPartKind kind)
{
    if (!m_registry) {
        return Entity::NULL_ID;
    }

    if (kind == UIEditorPartKind::Canvas) {
        return FindOrCreateCanvas();
    }

    const EntityID canvas = FindOrCreateCanvas();
    if (Entity::IsNull(canvas)) {
        return Entity::NULL_ID;
    }

    EntityID parent = canvas;
    if (kind == UIEditorPartKind::FillImage || kind == UIEditorPartKind::DamagePreview || kind == UIEditorPartKind::HPText) {
        parent = FindSelectedGaugeRoot();
        if (Entity::IsNull(parent)) {
            LOG_WARN("[UIEditor] Select or create a Gauge Root before adding HP-bound parts.");
            return Entity::NULL_ID;
        }
    } else if (kind == UIEditorPartKind::Image) {
        const EntityID root = FindSelectedGaugeRoot();
        parent = Entity::IsNull(root) ? canvas : root;
    }

    EntityID entity = ExecuteCreateSnapshot(*m_registry, UIEditorTemplates::BuildPartSnapshot(kind), parent, "Create UI Part");
    if (!Entity::IsNull(entity)) {
        SelectEntity(entity);
        SetStatusMessage(std::string(UIEditorTemplates::GetPartName(kind)) + " part created.");
    }
    return entity;
}

bool UIEditorPanel::SaveSelectedAsPrefab()
{
    if (!m_registry) {
        return false;
    }

    const EntityID root = FindSelectedGaugeRoot();
    if (Entity::IsNull(root)) {
        LOG_WARN("[UIEditor] Select an HP Gauge Root before saving a prefab.");
        SetStatusMessage("Select an HP Gauge Root before saving.", false);
        return false;
    }

    const bool validRoot =
        m_registry->GetComponent<HPGaugeBindingComponent>(root) &&
        m_registry->GetComponent<RectTransformComponent>(root) &&
        m_registry->GetComponent<CanvasItemComponent>(root) &&
        m_registry->GetComponent<TransformComponent>(root);
    bool hasFill = false;
    for (EntityID entity : CollectSubtree(*m_registry, root)) {
        if (m_registry->GetComponent<HPGaugeFillComponent>(entity)) {
            hasFill = true;
            break;
        }
    }
    if (!validRoot || !hasFill) {
        LOG_WARN("[UIEditor] Invalid HP Gauge prefab target.");
        SetStatusMessage("Invalid prefab target. Root needs HP Gauge components and at least one Fill.", false);
        return false;
    }

    std::filesystem::path outPath;
    if (!PrefabSystem::SaveEntityAsPrefab(root, *m_registry, PrefabDirectory(), &outPath)) {
        LOG_WARN("[UIEditor] Failed to save HP Gauge prefab.");
        SetStatusMessage("Failed to save HP Gauge prefab.", false);
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

    m_prefabState.lastPrefabPath = outPath;
    LOG_INFO("[UIEditor] Saved HP Gauge prefab: %s", outPath.string().c_str());
    SetStatusMessage("Saved HP Gauge prefab. Place it from Level Editor / SceneView.");
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
        SetStatusMessage("No prefab instance selected to apply.", false);
        return false;
    }

    const std::filesystem::path path = prefab->prefabAssetPath;
    const std::string beforeText = ReadText(path);
    const bool oldHasOverrides = prefab->hasOverrides;
    if (!PrefabSystem::ApplyPrefab(root, *m_registry)) {
        SetStatusMessage("Failed to apply UI prefab.", false);
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
    m_prefabState.lastPrefabPath = path;
    SetStatusMessage("Applied UI prefab.");
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
        SetStatusMessage("No prefab instance selected to revert.", false);
        return false;
    }

    const EntityID parent = GetParent(*m_registry, root);
    EntitySnapshot::Snapshot before = EntitySnapshot::CaptureSubtree(root, *m_registry);
    EntitySnapshot::Snapshot after = BuildPrefabInstanceSnapshot(prefab->prefabAssetPath);
    if (before.nodes.empty() || after.nodes.empty()) {
        SetStatusMessage("Failed to load prefab for revert.", false);
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
    SetStatusMessage("Reverted UI prefab.");
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
        SetStatusMessage("No prefab instance selected to unpack.", false);
        return false;
    }

    UndoSystem::Instance().ExecuteAction(
        std::make_unique<OptionalComponentUndoAction<PrefabInstanceComponent>>(
            root,
            std::optional<PrefabInstanceComponent>(*prefab),
            std::nullopt,
            "Unpack UI Prefab"),
        *m_registry);
    SetStatusMessage("Unpacked UI prefab.");
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

    EntityID fallback = Entity::NULL_ID;
    for (Archetype* archetype : m_registry->GetAllArchetypes()) {
        for (EntityID entity : archetype->GetEntities()) {
            if (!m_registry->IsAlive(entity)) {
                continue;
            }

            auto* rect = m_registry->GetComponent<RectTransformComponent>(entity);
            auto* canvas = m_registry->GetComponent<CanvasItemComponent>(entity);
            if (!rect || !canvas) {
                continue;
            }

            if (auto* name = m_registry->GetComponent<NameComponent>(entity)) {
                if (name->name == UIEditorTemplates::kAuthoringCanvasName || name->name == "BattleHUD_Canvas") {
                    return entity;
                }
            }

            if (Entity::IsNull(fallback) && !m_registry->GetComponent<HPGaugeBindingComponent>(entity)) {
                fallback = entity;
            }
        }
    }
    return fallback;
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

void UIEditorPanel::SetStatusMessage(const std::string& message, bool success, float seconds)
{
    m_interactionState.statusMessage = message;
    m_interactionState.statusSuccess = success;
    m_interactionState.statusTimer = (std::max)(0.1f, seconds);
}

void UIEditorPanel::SyncRectEdit(EntityID entity)
{
    if (!m_registry || Entity::IsNull(entity) || !m_registry->IsAlive(entity)) {
        return;
    }

    auto* rect = m_registry->GetComponent<RectTransformComponent>(entity);
    auto* transform = m_registry->GetComponent<TransformComponent>(entity);
    if (!rect || !transform) {
        return;
    }

    Editor2D::SyncRectTransformToTransform(*rect, *transform);
    HierarchySystem::MarkDirtyRecursive(entity, *m_registry);
    HierarchySystem hierarchySystem;
    hierarchySystem.Update(*m_registry);
}

void UIEditorPanel::CommitDesignerRectEdit()
{
    if (!m_registry || (!m_interactionState.draggingRect && !m_interactionState.resizingRect)) {
        return;
    }
    if (ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
        return;
    }

    const EntityID entity = m_interactionState.editEntity;
    if (!Entity::IsNull(entity) && m_registry->IsAlive(entity)) {
        if (auto* rect = m_registry->GetComponent<RectTransformComponent>(entity)) {
            if (HasComponentChange(m_interactionState.beforeRect, *rect)) {
                RecordComponentChange(*m_registry, entity, m_interactionState.beforeRect, *rect);
            }
        }
    }

    m_interactionState.draggingRect = false;
    m_interactionState.resizingRect = false;
    m_interactionState.editEntity = Entity::NULL_ID;
}
