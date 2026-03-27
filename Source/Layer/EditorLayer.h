#pragma once
#include "Layer.h"
#include "GameLayer.h"
#include "Asset/AssetBrowser.h"
#include <memory>
#include <DirectXMath.h>
#include <string>
#include <filesystem>

class ITexture;

class EditorLayer : public Layer
{
public:
    enum class GizmoOperation
    {
        Translate,
        Rotate,
        Scale
    };

    enum class GizmoSpace
    {
        Local,
        World
    };

    enum class PendingSceneAction
    {
        None,
        NewScene,
        OpenSceneDialog,
        LoadScenePath
    };

    EditorLayer(GameLayer* gameLayer);
    ~EditorLayer() override = default;

    void Initialize() override;
    void Finalize() override;
    void Update(const EngineTime& time) override;
    void RenderUI() override;

    AssetBrowser* GetAssetBrowser() const { return m_assetBrowser.get(); }
    DirectX::XMFLOAT2 GetSceneViewSize() const { return m_sceneViewSize; }
    DirectX::XMFLOAT2 GetGameViewSize() const { return m_gameViewSize; }
    DirectX::XMFLOAT4 GetSceneViewRect() const { return m_sceneViewRect; }
    DirectX::XMFLOAT3 GetEditorCameraPosition() const { return m_editorCameraPosition; }
    DirectX::XMFLOAT3 GetEditorCameraDirection() const;
    DirectX::XMFLOAT4X4 GetEditorViewMatrix() const;
    DirectX::XMFLOAT4X4 BuildEditorProjectionMatrix(float aspect) const;
    float GetEditorCameraFovY() const { return m_editorCameraFovY; }
    bool HasEditorCameraUserOverride() const { return m_editorCameraUserOverride; }
    bool HasEditorCameraAutoFramed() const { return m_editorCameraAutoFramed; }
    void SetEditorCameraLookAt(const DirectX::XMFLOAT3& position, const DirectX::XMFLOAT3& target);
    void SetSceneViewTexture(ITexture* texture) { m_sceneViewTexture = texture; }
    void SetGBufferDebugTextures(ITexture* g0, ITexture* g1, ITexture* g2, ITexture* g3, ITexture* depth) {
        m_gbufferTexture0 = g0;
        m_gbufferTexture1 = g1;
        m_gbufferTexture2 = g2;
        m_gbufferTexture3 = g3;
        m_gbufferDepthTexture = depth;
    }
private:
    GameLayer* m_gameLayer;
    std::unique_ptr<AssetBrowser> m_assetBrowser;

    EntityID m_selectedEntity = Entity::NULL_ID;
    bool m_showLightingWindow = false;
    bool m_showGBufferDebug = false;
    DirectX::XMFLOAT2 m_sceneViewSize = { 0.0f, 0.0f };
    DirectX::XMFLOAT2 m_gameViewSize = { 0.0f, 0.0f };
    DirectX::XMFLOAT4 m_sceneViewRect = { 0.0f, 0.0f, 0.0f, 0.0f };
    DirectX::XMFLOAT3 m_editorCameraPosition = { 0.0f, 12.0f, -80.0f };
    float m_editorCameraYaw = 0.0f;
    float m_editorCameraPitch = 0.0f;
    float m_editorCameraFovY = 0.785398f;
    bool m_sceneViewHovered = false;
    bool m_sceneViewToolbarHovered = false;
    ITexture* m_sceneViewTexture = nullptr;
    ITexture* m_gbufferTexture0 = nullptr;
    ITexture* m_gbufferTexture1 = nullptr;
    ITexture* m_gbufferTexture2 = nullptr;
    ITexture* m_gbufferTexture3 = nullptr;
    ITexture* m_gbufferDepthTexture = nullptr;
    bool m_editorCameraUserOverride = false;
    bool m_editorCameraAutoFramed = false;
    GizmoOperation m_gizmoOperation = GizmoOperation::Translate;
    GizmoSpace m_gizmoSpace = GizmoSpace::Local;
    bool m_gizmoWasUsing = false;
    bool m_gizmoIsOver = false;
    EntityID m_gizmoUndoEntity = Entity::NULL_ID;
    bool m_hasGizmoBeforeTransform = false;
    bool m_scenePickPending = false;
    bool m_scenePickBlockedByGizmo = false;
    DirectX::XMFLOAT2 m_scenePickStart = { 0.0f, 0.0f };
    std::string m_sceneSavePath;
    float m_cameraMoveSpeed = 20.0f;
    bool m_translateSnapEnabled = false;
    bool m_rotateSnapEnabled = false;
    bool m_scaleSnapEnabled = false;
    float m_translateSnapStep = 0.5f;
    float m_rotateSnapStep = 15.0f;
    float m_scaleSnapStep = 0.1f;
    uint64_t m_savedSceneRevision = 0;
    bool m_openUnsavedChangesPopup = false;
    PendingSceneAction m_pendingSceneAction = PendingSceneAction::None;
    std::filesystem::path m_pendingSceneLoadPath;
    // ==========================================
    // ==========================================
    void DrawDockSpace();
    void DrawMenuBar();
    void DrawMainToolbar();
    void DrawSceneView();
    void DrawGameView();
    void DrawHierarchy();
    void DrawInspector();
    void DrawLightingWindow();
    void DrawGBufferDebugWindow();
    void DrawStatusBar();
    void DrawUnsavedChangesPopup();
    void HandleEditorShortcuts();
    void DrawSceneViewToolbar();
    void DrawTransformGizmo();
    void HandleScenePicking();
    void HandleSceneAssetDrop();
    void FocusSelectedEntity();
    void FocusEditorCameraOnTarget(const DirectX::XMFLOAT3& target, float radius);
    void SetEditorCameraDirection(const DirectX::XMFLOAT3& forward, const DirectX::XMFLOAT3& target, float distance);
    void ProcessDeferredEditorActions();
    bool IsSceneDirty() const;
    void MarkSceneSaved();
    void RequestSceneAction(PendingSceneAction action, std::filesystem::path scenePath = {});
    bool ExecutePendingSceneAction();
    void NewScene();
    bool LoadSceneFromPath(const std::filesystem::path& scenePath);
    bool OpenScene();
    bool SaveCurrentScene();
    bool SaveCurrentSceneAs();

    bool m_requestNewScene = false;
    bool m_requestOpenScene = false;
    bool m_requestSaveSceneAs = false;
};
