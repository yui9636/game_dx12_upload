#pragma once

#include <filesystem>
#include <string>

#include "Entity/Entity.h"
#include "UIEditor/UIEditorState.h"

class Registry;

class UIEditorPanel
{
public:
    void DrawWorkspace(Registry* registry, bool* outFocused);

private:
    void DrawToolbar();
    void DrawPalette();
    void DrawDesignerView();
    void DrawWidgetTree();
    void DrawProperties();
    void DrawPrefabBar();
    void DrawPreviewHPPanel(EntityID gaugeRoot);
    void DrawStatusLine();

    EntityID FindOrCreateCanvas();
    EntityID CreateTemplate(UIEditorTemplateKind kind);
    EntityID CreatePart(UIEditorPartKind kind);
    bool SaveSelectedAsPrefab();
    bool ApplySelectedPrefab();
    bool RevertSelectedPrefab();
    bool UnpackSelectedPrefab();

    EntityID ResolveGaugeRoot(EntityID entity) const;
    EntityID FindSelectedGaugeRoot() const;
    EntityID FindCanvas() const;
    void SelectEntity(EntityID entity);
    void SyncRectEdit(EntityID entity);
    void CommitDesignerRectEdit();
    void SetStatusMessage(const std::string& message, bool success = true, float seconds = 4.0f);

    Registry* m_registry = nullptr;
    EntityID m_selectedEntity = Entity::NULL_ID;
    EntityID m_selectedGaugeRoot = Entity::NULL_ID;
    UIEditorViewState m_viewState;
    UIEditorHPPreviewState m_previewState;
    UIEditorInteractionState m_interactionState;
    UIEditorPrefabState m_prefabState;
};
