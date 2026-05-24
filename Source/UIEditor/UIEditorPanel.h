#pragma once

#include <filesystem>
#include <string>

#include <DirectXMath.h>

#include "Entity/Entity.h"
#include "UIEditor/UIEditorState.h"
// Registry はこの機能の公開インターフェースを定義し、実装側が具体的な処理を行う。

class Registry;

class UIEditorPanel
{
public:
    void DrawWorkspace(Registry* registry, bool* outFocused);

    // ---- Automation API ----
    void     SetRegistryForAutomation(Registry* registry)        { m_registry = registry; }
    DirectX::XMFLOAT4 GetWorkspaceRectForAutomation()   const   { return m_workspaceRect; }
    EntityID GetSelectedEntityAutomation()               const   { return m_selectedEntity; }
    void     SelectEntityAutomation(EntityID entity)             { SelectEntity(entity); }
    EntityID FindCanvasAutomation()                      const   { return FindCanvas(); }
    EntityID FindOrCreateCanvasAutomation()                      { return FindOrCreateCanvas(); }
    EntityID CreateTemplateAutomation(UIEditorTemplateKind kind) { return CreateTemplate(kind); }
    EntityID CreatePartAutomation(UIEditorPartKind kind)         { return CreatePart(kind); }
    bool     SaveSelectedAsPrefabAutomation()                    { return SaveSelectedAsPrefab(); }
    bool     ApplySelectedPrefabAutomation()                     { return ApplySelectedPrefab(); }
    bool     RevertSelectedPrefabAutomation()                    { return RevertSelectedPrefab(); }
    bool     UnpackSelectedPrefabAutomation()                    { return UnpackSelectedPrefab(); }
    const UIEditorViewState&   GetViewState()            const   { return m_viewState; }
    UIEditorViewState&         GetViewStateMutable()             { return m_viewState; }
    const UIEditorPrefabState& GetPrefabState()          const   { return m_prefabState; }

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
    DirectX::XMFLOAT4 m_workspaceRect = { 0.0f, 0.0f, 0.0f, 0.0f };
    EntityID m_selectedEntity = Entity::NULL_ID;
    EntityID m_selectedGaugeRoot = Entity::NULL_ID;
    EntityID m_treeExpandTarget = Entity::NULL_ID;
    EntityID m_pendingFrameEntity = Entity::NULL_ID;
    UIEditorViewState m_viewState;
    UIEditorHPPreviewState m_previewState;
    UIEditorInteractionState m_interactionState;
    UIEditorPrefabState m_prefabState;
};
