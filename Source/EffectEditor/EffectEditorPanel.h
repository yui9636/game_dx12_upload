#pragma once

#include <memory>
#include <string>
#include <vector>
#include <DirectXMath.h>
#include "Entity/Entity.h"
#include "EffectRuntime/EffectGraphAsset.h"

struct ImVec2;
class Registry;
class ITexture;
struct EffectParameterOverrideComponent;

namespace ax::NodeEditor
{
    struct EditorContext;
}

class EffectEditorPanel
{
    friend class EffectEditorTemplates;
public:
    EffectEditorPanel();
    ~EffectEditorPanel();

    void DrawWorkspace(Registry* registry, bool* outFocused);
    void SetViewportTexture(ITexture* texture) { m_viewportTexture = texture; }
    void SetSelectedContext(EntityID entity, const std::string& meshPath);
    DirectX::XMFLOAT2 GetPreviewRenderSize() const { return m_previewRenderSize; }
    DirectX::XMFLOAT3 GetPreviewCameraPosition() const;
    DirectX::XMFLOAT3 GetPreviewCameraTarget() const { return m_previewAnchor; }
    DirectX::XMFLOAT3 GetPreviewCameraDirection() const;
    float GetPreviewCameraFovY() const { return m_previewFovY; }
    float GetPreviewNearZ() const { return m_previewNearZ; }
    float GetPreviewFarZ() const { return m_previewFarZ; }
    DirectX::XMFLOAT4 GetPreviewClearColor() const { return m_previewClearColor; }
    bool ShouldPreviewUseSkybox() const { return m_previewUseSkybox; }

private:
    enum class AssetPickerKind : uint8_t
    {
        None = 0,
        Mesh,
        Texture
    };

    enum class AuthoringMode : uint8_t
    {
        Stack = 0,
        Node
    };

    enum class AssetPickerView : uint8_t
    {
        All = 0,
        Favorites,
        Recent
    };

    void BuildDockLayout(unsigned int dockId);
    void DrawToolbar();
    void DrawGraphPanel();
    void DrawPreviewPanel();
    void DrawTimelinePanel();
    void DrawDetailsPanel();
    void DrawCompiledPanel();
    void DrawAssetPanel();
    void DrawGuiPanel();
    void DrawNodeModePanel();
    void ResetPreviewCamera();

    bool CompileDocument();
    void NewDocument();
    bool SaveDocument();
    bool LoadDocument();
    std::string BuildTransientAssetKey() const;
    std::string GetActiveAssetKey() const;
    void QueuePreviewSpawn();
    void QueuePreviewSpawnAt(float startTime, bool pausedOnSpawn);
    void StopPreview();
    void RemoveNode(uint32_t nodeId);
    bool CanCreateLink(uint32_t startPinId, uint32_t endPinId, std::string& reason) const;
    void DrawAssetPickerPopup();
    void OpenAssetPicker(AssetPickerKind kind, uint32_t nodeId, bool targetPreviewMesh);
    bool DrawAssetSlotControl(const char* label, std::string& path, AssetPickerKind kind, uint32_t nodeId, bool targetPreviewMesh);
    std::string* GetAssetPickerTargetPath();
    void AssignPickedAsset(const std::string& path);
    void TouchRecentAsset(const std::string& path, AssetPickerKind kind);
    bool IsFavoriteAsset(const std::string& path, AssetPickerKind kind) const;
    void ToggleFavoriteAsset(const std::string& path, AssetPickerKind kind);
    bool IsCompatibleAssetPath(const std::string& path, AssetPickerKind kind) const;
    bool AcceptAssetDropPayload(std::string& path, AssetPickerKind kind);
    EffectParameterOverrideComponent BuildSuggestedOverrideComponent() const;
    void EnsureRuntimeOverrideComponent(EntityID entity);
    void DrawRuntimeOverrideSection(const char* label, EntityID entity, bool autoAttach);
    EffectGraphNode* FindNodeByType(EffectGraphNodeType type);
    EffectGraphNode* EnsureNodeByType(EffectGraphNodeType type);

    Registry* m_registry = nullptr;
    ITexture* m_viewportTexture = nullptr;
    EntityID m_selectedEntity = Entity::NULL_ID;
    std::string m_selectedMeshPath;
    EntityID m_previewEntity = Entity::NULL_ID;

    EffectGraphAsset m_asset;
    std::shared_ptr<CompiledEffectAsset> m_compiled;
    bool m_compileDirty = true;
    bool m_needsLayoutRebuild = true;
    bool m_syncNodePositions = true;
    bool m_scrubResumesPlay = true;
    bool m_scrubWasPlaying = false;
    uint32_t m_selectedNodeId = 0;
    uint32_t m_selectedLinkId = 0;
    DirectX::XMFLOAT2 m_pendingNodePopupPos = { 0.0f, 0.0f };
    DirectX::XMFLOAT2 m_previewRenderSize = { 1280.0f, 720.0f };
    DirectX::XMFLOAT3 m_previewAnchor = { 10000.0f, 1.25f, 10000.0f };
    float m_previewYaw = 0.85f;
    float m_previewPitch = -0.18f;
    float m_previewDistance = 4.5f;
    float m_previewFovY = 0.785398f;
    float m_previewNearZ = 0.03f;
    float m_previewFarZ = 2048.0f;
    DirectX::XMFLOAT4 m_previewClearColor = { 0.22f, 0.22f, 0.23f, 1.0f };
    bool m_previewHovered = false;
    bool m_previewUseSkybox = false;
    bool m_showBlackboard = false;
    AssetPickerKind m_assetPickerKind = AssetPickerKind::None;
    AuthoringMode m_authoringMode = AuthoringMode::Stack;
    bool m_assetPickerOpenRequested = false;
    bool m_assetPickerTargetsPreviewMesh = false;
    uint32_t m_assetPickerNodeId = 0;
    AssetPickerView m_assetPickerView = AssetPickerView::All;
    char m_assetPickerSearch[128] = {};
    std::vector<std::string> m_recentMeshAssets;
    std::vector<std::string> m_recentTextureAssets;
    std::vector<std::string> m_favoriteMeshAssets;
    std::vector<std::string> m_favoriteTextureAssets;

    std::string m_documentPath = "Data/EffectGraph/Untitled.effectgraph.json";
    char m_documentPathBuffer[260] = {};

    ax::NodeEditor::EditorContext* m_nodeEditorContext = nullptr;
};
