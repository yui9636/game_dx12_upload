#pragma once

#include <filesystem>
#include <string>

#include "Entity/Entity.h"

class Registry;

class UIEditorPanel
{
public:
    enum class TemplateKind
    {
        PlayerHP,
        BossHP
    };

    enum class PartKind
    {
        Canvas,
        GaugeRoot,
        Image,
        FillImage,
        DamagePreview,
        HPText
    };

    enum class PlacementPreset
    {
        TopLeft,
        TopCenter,
        TopRight,
        MiddleLeft,
        MiddleRight,
        BottomLeft,
        BottomRight,
        Custom
    };

    void DrawWorkspace(Registry* registry, bool* outFocused);

private:
    void DrawToolbar();
    void DrawPalette();
    void DrawDesignerView();
    void DrawWidgetTree();
    void DrawProperties();
    void DrawPrefabBar();

    EntityID FindOrCreateCanvas();
    EntityID CreateTemplate(TemplateKind kind);
    EntityID CreatePart(PartKind kind);
    EntityID InstantiatePrefab(const std::filesystem::path& path);
    bool SaveSelectedAsPrefab();
    bool ApplySelectedPrefab();
    bool RevertSelectedPrefab();
    bool UnpackSelectedPrefab();

    EntityID ResolveGaugeRoot(EntityID entity) const;
    EntityID FindSelectedGaugeRoot() const;
    EntityID FindCanvas() const;
    void SelectEntity(EntityID entity);
    void ApplyPlacementPreset(EntityID entity, PlacementPreset preset);

    Registry* m_registry = nullptr;
    EntityID m_selectedEntity = Entity::NULL_ID;
    EntityID m_selectedGaugeRoot = Entity::NULL_ID;
    std::filesystem::path m_lastPrefabPath;
    PlacementPreset m_lastPlacementPreset = PlacementPreset::TopLeft;
    bool m_showSafeArea = true;
    bool m_pixelSnap = true;
    bool m_showGrid = true;
};
