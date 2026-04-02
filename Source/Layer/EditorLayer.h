#pragma once
#include "Layer.h"
#include "GameLayer.h"
#include "Asset/AssetBrowser.h"
#include "Input/EditorInputBridge.h"
#include "PlayerEditor/PlayerEditorPanel.h"
#include "PlayerEditor/PlayerEditorWindow.h"
#include <memory>
#include <array>
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

    enum class SceneViewMode
    {
        Mode3D,
        Mode2D
    };

    enum class GameViewResolutionPreset
    {
        Free,
        HD1080,
        HD720,
        Portrait1080x1920,
        Portrait750x1334
    };

    enum class GameViewAspectPolicy
    {
        Fit,
        Fill,
        PixelPerfect
    };

    enum class GameViewScalePolicy
    {
        AutoFit,
        Scale1x,
        Scale2x,
        Scale3x
    };

    enum class SceneShadingMode
    {
        Lit,
        Unlit,
        Wireframe
    };

    enum class PendingSceneAction
    {
        None,
        NewScene,
        OpenSceneDialog,
        LoadScenePath
    };

    enum class WindowFocusTarget
    {
        None,
        SceneView,
        GameView,
        Hierarchy,
        Inspector,
        AssetBrowser,
        Console,
        Lighting,
        Audio,
        RenderPasses,
        GridSettings,
        GBufferDebug,
        PlayerEditor
    };

    enum class WorkspaceTab
    {
        LevelEditor,
        PlayerEditor
    };

    struct CameraBookmark
    {
        bool valid = false;
        SceneViewMode mode = SceneViewMode::Mode3D;
        DirectX::XMFLOAT3 cameraPosition = { 0.0f, 12.0f, -80.0f };
        float cameraYaw = 0.0f;
        float cameraPitch = 0.0f;
        DirectX::XMFLOAT2 center2D = { 0.0f, 0.0f };
        float zoom2D = 10.0f;
    };

    EditorLayer(GameLayer* gameLayer);
    ~EditorLayer() override = default;

    void Initialize() override;
    void Finalize() override;
    void Update(const EngineTime& time) override;
    void RenderUI() override;
    void RenderDetachedWindows();

    AssetBrowser* GetAssetBrowser() const { return m_assetBrowser.get(); }
    EditorInputBridge& GetInputBridge() { return m_inputBridge; }
    DirectX::XMFLOAT2 GetSceneViewSize() const { return m_sceneViewSize; }
    DirectX::XMFLOAT2 GetGameViewSize() const { return m_gameViewSize; }
    DirectX::XMFLOAT4 GetSceneViewRect() const { return m_sceneViewRect; }
    bool ShouldRenderSceneGrid3D() const { return m_showSceneGrid && m_sceneViewMode == SceneViewMode::Mode3D; }
    DirectX::XMFLOAT3 GetEditorCameraPosition() const {
        return (m_sceneViewMode == SceneViewMode::Mode2D)
            ? DirectX::XMFLOAT3{ m_editor2DCenter.x, m_editor2DCenter.y, -100.0f }
            : m_editorCameraPosition;
    }
    DirectX::XMFLOAT3 GetEditorCameraDirection() const;
    DirectX::XMFLOAT4X4 GetEditorViewMatrix() const;
    DirectX::XMFLOAT4X4 BuildEditorProjectionMatrix(float aspect) const;
    float GetEditorCameraFovY() const { return m_editorCameraFovY; }
    bool HasEditorCameraUserOverride() const { return m_editorCameraUserOverride; }
    bool HasEditorCameraAutoFramed() const { return m_editorCameraAutoFramed; }
    void SetEditorCameraLookAt(const DirectX::XMFLOAT3& position, const DirectX::XMFLOAT3& target);
    void SetSceneViewTexture(ITexture* texture) { m_sceneViewTexture = texture; }
    void SetGameViewTexture(ITexture* texture) { m_gameViewTexture = texture; }
    SceneViewMode GetSceneViewMode() const { return m_sceneViewMode; }
    float GetSceneGridCellSize() const { return m_sceneGridCellSize; }
    int GetSceneGridHalfLineCount() const { return m_sceneGridHalfLineCount; }
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
    EditorInputBridge m_inputBridge;

    EntityID m_selectedEntity = Entity::NULL_ID;
    bool m_showSceneView = true;
    bool m_showGameView = true;
    bool m_showHierarchy = true;
    bool m_showInspector = true;
    bool m_showAssetBrowser = true;
    bool m_showConsole = true;
    bool m_showLightingWindow = false;
    bool m_showAudioWindow = false;
    bool m_showRenderPassesWindow = false;
    bool m_showGridSettingsWindow = false;
    bool m_showGBufferDebug = false;
    bool m_showStatusBar = true;
    bool m_showMainToolbar = true;
    bool m_showSceneGrid = true;
    float m_sceneGridCellSize = 20.0f;
    int m_sceneGridHalfLineCount = 32;
    bool m_showSceneGizmo = true;
    bool m_showSceneStatsOverlay = false;
    bool m_showSceneSelectionOutline = false;
    bool m_showSceneLightIcons = true;
    bool m_showSceneCameraIcons = true;
    bool m_showSceneBounds = false;
    bool m_showSceneCollision = false;
    bool m_showInputDebug = false;
    bool m_showPlayerEditor = false;
    WorkspaceTab m_activeWorkspace = WorkspaceTab::LevelEditor;
    PlayerEditorPanel m_playerEditorPanel;
    std::unique_ptr<PlayerEditorWindow> m_playerEditorWindow;
    SceneShadingMode m_sceneShadingMode = SceneShadingMode::Lit;
    DirectX::XMFLOAT2 m_sceneViewSize = { 0.0f, 0.0f };
    DirectX::XMFLOAT2 m_gameViewSize = { 0.0f, 0.0f };
    DirectX::XMFLOAT4 m_sceneViewRect = { 0.0f, 0.0f, 0.0f, 0.0f };
    DirectX::XMFLOAT4 m_gameViewRect = { 0.0f, 0.0f, 0.0f, 0.0f };
    DirectX::XMFLOAT3 m_editorCameraPosition = { 0.0f, 12.0f, -80.0f };
    float m_editorCameraYaw = 0.0f;
    float m_editorCameraPitch = 0.0f;
    float m_editorCameraFovY = 0.785398f;
    bool m_sceneViewHovered = false;
    bool m_sceneViewToolbarHovered = false;
    ITexture* m_sceneViewTexture = nullptr;
    ITexture* m_gameViewTexture = nullptr;
    ITexture* m_gbufferTexture0 = nullptr;
    ITexture* m_gbufferTexture1 = nullptr;
    ITexture* m_gbufferTexture2 = nullptr;
    ITexture* m_gbufferTexture3 = nullptr;
    ITexture* m_gbufferDepthTexture = nullptr;
    bool m_editorCameraUserOverride = false;
    bool m_editorCameraAutoFramed = false;
    SceneViewMode m_sceneViewMode = SceneViewMode::Mode3D;
    DirectX::XMFLOAT2 m_editor2DCenter = { 0.0f, 0.0f };
    float m_editor2DZoom = 10.0f;
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
    bool m_openRecoveryPopup = false;
    PendingSceneAction m_pendingSceneAction = PendingSceneAction::None;
    std::filesystem::path m_pendingSceneLoadPath;
    std::filesystem::path m_pendingRecoveryAutosavePath;
    std::filesystem::path m_pendingRecoveryScenePath;
    bool m_hasCheckedRecovery = false;
    double m_autosaveAccumulator = 0.0;
    // ==========================================
    // ==========================================
    void DrawDockSpace();
    void DrawWorkspaceTabs();
    void DrawMenuBar();
    void DrawMainToolbar();
    void DrawPlayerEditorWorkspace();
    void DrawSceneView();
    void DrawGameView();
    void DrawHierarchy();
    void DrawInspector();
    void DrawLightingWindow();
    void DrawAudioWindow();
    void DrawRenderPassesWindow();
    void DrawGridSettingsWindow();
    void DrawGBufferDebugWindow();
    void DrawStatusBar();
    void DrawUnsavedChangesPopup();
    void DrawRecoveryPopup();
    void SyncPlayerEditorPanelState();
    void HandleEditorShortcuts();
    void DrawSceneViewToolbar();
    void DrawTransformGizmo();
    void Draw2DOverlay();
    void Draw2DOverlayForRect(const DirectX::XMFLOAT4& viewRect,
                              const DirectX::XMFLOAT4X4& view,
                              const DirectX::XMFLOAT4X4& projection,
                              bool drawSelection);
    bool TryBuildGameView2DViewProjection(DirectX::XMFLOAT4X4& outView,
                                          DirectX::XMFLOAT4X4& outProjection) const;
    void HandleScenePicking();
    void HandleSceneAssetDrop();
    void FocusSelectedEntity();
    void FocusEditorCameraOnTarget(const DirectX::XMFLOAT3& target, float radius);
    void SetEditorCameraDirection(const DirectX::XMFLOAT3& forward, const DirectX::XMFLOAT3& target, float distance);
    void ProcessDeferredEditorActions();
    bool IsSceneDirty() const;
    void MarkSceneSaved();
    void UpdateAutosave(float deltaSeconds);
    void CheckRecoveryCandidate();
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
    GameViewResolutionPreset m_gameViewResolutionPreset = GameViewResolutionPreset::Free;
    GameViewAspectPolicy m_gameViewAspectPolicy = GameViewAspectPolicy::Fit;
    GameViewScalePolicy m_gameViewScalePolicy = GameViewScalePolicy::AutoFit;
    bool m_gameViewShowSafeArea = false;
    bool m_gameViewShowPixelPreview = false;
    bool m_gameViewShowStatsOverlay = false;
    bool m_gameViewShowUIOverlay = true;
    bool m_gameViewShow2DOverlay = true;
    bool m_forceDockLayoutReset = false;
    bool m_requestRenamePopup = false;
    char m_renameBuffer[256] = {};
    WindowFocusTarget m_pendingWindowFocus = WindowFocusTarget::None;
    WindowFocusTarget m_lastFocusedWindow = WindowFocusTarget::None;
    WindowFocusTarget m_maximizedWindow = WindowFocusTarget::None;
    std::array<CameraBookmark, 3> m_cameraBookmarks{};

    void ExecuteUndo();
    void ExecuteRedo();
    void ExecuteDuplicateSelection();
    void ExecuteDeleteSelection();
    void ExecuteFrameSelected();
    void ExecuteSelectAll();
    void ExecuteDeselect();
    void ExecuteRenameSelection();
    void ExecuteCopySelection();
    void ExecutePasteSelection();
    void ExecuteResetTransform();
    void ExecuteCreateEmptyParent();
    void ExecuteUnpackPrefab();
    void ExecuteFocusSearch();
    void ExecuteResetView();
    void ExecuteGamePlay();
    void ExecuteGamePauseToggle();
    void ExecuteGameStop();
    void ExecuteGameStep();
    void ExecuteGameResetPreview();
    void ExecuteCloseSecondaryWindows();
    void ExecuteResetLayout();
    void ExecuteMaximizeActivePanel();
    void RequestWindowFocus(WindowFocusTarget target);
    void ApplyPendingWindowFocus(WindowFocusTarget target);
    void SetLastFocusedWindow(WindowFocusTarget target, bool focused);
    void SaveCameraBookmark(size_t slot);
    void LoadCameraBookmark(size_t slot);
    void DrawRenamePopup();
    void DrawSceneGridOverlay(const DirectX::XMFLOAT4& viewRect,
                              const DirectX::XMFLOAT4X4& view,
                              const DirectX::XMFLOAT4X4& projection) const;
    void DrawSelectionOutlineOverlay(const DirectX::XMFLOAT4& viewRect,
                                     const DirectX::XMFLOAT4X4& view,
                                     const DirectX::XMFLOAT4X4& projection) const;
    void DrawStatsOverlay(const DirectX::XMFLOAT4& viewRect, const char* label) const;
    void DrawSceneIconOverlay(const DirectX::XMFLOAT4& viewRect,
                              const DirectX::XMFLOAT4X4& view,
                              const DirectX::XMFLOAT4X4& projection) const;
    void DrawSceneBoundsOverlay(const DirectX::XMFLOAT4& viewRect,
                                const DirectX::XMFLOAT4X4& view,
                                const DirectX::XMFLOAT4X4& projection) const;
    void DrawSceneCollisionOverlay(const DirectX::XMFLOAT4& viewRect,
                                   const DirectX::XMFLOAT4X4& view,
                                   const DirectX::XMFLOAT4X4& projection) const;
};
