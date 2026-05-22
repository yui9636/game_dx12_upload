#pragma once
#include "Layer.h"
#include "GameLayer.h"
#include "Asset/AssetBrowser.h"
#include "Asset/ModelSerializerPanel.h"
#include "Input/EditorInputBridge.h"
#include "PlayerEditor/PlayerEditorPanel.h"
#include "PlayerEditor/PlayerEditorWindow.h"
#include "EffectEditor/EffectEditorPanel.h"
#include "GameLoop/GameLoopEditorPanel.h"
#include "Sequencer/SequencerPanel.h"
#include "UIEditor/UIEditorPanel.h"
#include "Terrain/TerrainEditorPanel.h"
#include "Terrain/CreateTerrainDialog.h"
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
        Serializer,
        Console,
        Sequencer,
        Lighting,
        Audio,
        RenderPasses,
        GridSettings,
        GBufferDebug,
        PlayerEditor,
        EffectEditor,
        UIEditor,
        GameLoopEditor
    };

    enum class WorkspaceTab
    {
        LevelEditor,
        PlayerEditor,
        EffectEditor,
        UIEditor
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

    // PlayerEditor から editor camera に適用する shake オフセットを設定する。
    void SetPlayerEditorCameraShakeOffset(const DirectX::XMFLOAT3& offset);

    // PlayerEditor 用 camera shake オフセットをクリアする。
    void ClearPlayerEditorCameraShakeOffset();



    AssetBrowser* GetAssetBrowser() const { return m_assetBrowser.get(); }
    EditorInputBridge& GetInputBridge() { return m_inputBridge; }
    DirectX::XMFLOAT2 GetSceneViewSize() const { return m_sceneViewSize; }
    DirectX::XMFLOAT2 GetGameViewSize() const { return m_gameViewSize; }
    DirectX::XMFLOAT4 GetSceneViewRect() const { return m_sceneViewRect; }
    DirectX::XMFLOAT4 GetGameViewRect() const { return m_gameViewRect; }
    bool ShouldRenderSceneGrid3D() const {
        const bool isGridWorkspace =
            m_activeWorkspace == WorkspaceTab::LevelEditor ||
            m_activeWorkspace == WorkspaceTab::PlayerEditor;

        return isGridWorkspace &&
            !m_showTerrainEditor &&
            IsSceneGridVisible() &&
            m_sceneViewMode == SceneViewMode::Mode3D;
    }

    // scene grid の表示状態は 2D mode と 3D mode で別々に保持する。
    bool IsSceneGridVisible() const {
        return (m_sceneViewMode == SceneViewMode::Mode2D) ? m_showSceneGrid2D : m_showSceneGrid3D;
    }
    void SetSceneGridVisible(bool value) {
        if (m_sceneViewMode == SceneViewMode::Mode2D) m_showSceneGrid2D = value;
        else m_showSceneGrid3D = value;
    }

    DirectX::XMFLOAT3 GetEditorCameraPosition() const {

        if (m_sceneViewMode == SceneViewMode::Mode2D) {
            return DirectX::XMFLOAT3{
                m_editor2DCenter.x + m_editorCameraShakeOffset.x,
                m_editor2DCenter.y + m_editorCameraShakeOffset.y,
                -100.0f + m_editorCameraShakeOffset.z
            };
        }

        return DirectX::XMFLOAT3{
            m_editorCameraPosition.x + m_editorCameraShakeOffset.x,
            m_editorCameraPosition.y + m_editorCameraShakeOffset.y,
            m_editorCameraPosition.z + m_editorCameraShakeOffset.z
        };


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
    void SetPlayerPreviewTexture(ITexture* texture) { m_playerEditorPanel.SetViewportTexture(texture); }
    void SetEffectPreviewTexture(ITexture* texture) { m_effectEditorPanel.SetViewportTexture(texture); }
    PlayerEditorPanel& GetPlayerEditorPanel() { return m_playerEditorPanel; }
    const PlayerEditorPanel& GetPlayerEditorPanel() const { return m_playerEditorPanel; }
    bool IsPlayerWorkspaceActive() const { return m_showPlayerEditor && m_activeWorkspace == WorkspaceTab::PlayerEditor; }
    bool ShouldRenderPlayerPreview() const { return IsPlayerWorkspaceActive() && m_playerEditorPanel.CanRenderPreview(); }
    DirectX::XMFLOAT2 GetPlayerPreviewRenderSize() const { return m_playerEditorPanel.GetPreviewRenderSize(); }
    DirectX::XMFLOAT3 GetPlayerPreviewCameraPosition() const { return m_playerEditorPanel.GetPreviewCameraPosition(); }
    DirectX::XMFLOAT3 GetPlayerPreviewCameraTarget() const { return m_playerEditorPanel.GetPreviewCameraTarget(); }
    DirectX::XMFLOAT3 GetPlayerPreviewCameraDirection() const { return m_playerEditorPanel.GetPreviewCameraDirection(); }
    float GetPlayerPreviewCameraFovY() const { return m_playerEditorPanel.GetPreviewCameraFovY(); }
    float GetPlayerPreviewNearZ() const { return m_playerEditorPanel.GetPreviewNearZ(); }
    float GetPlayerPreviewFarZ() const { return m_playerEditorPanel.GetPreviewFarZ(); }
    DirectX::XMFLOAT4 GetPlayerPreviewClearColor() const { return m_playerEditorPanel.GetPreviewClearColor(); }
    bool ShouldPlayerPreviewUseSkybox() const { return m_playerEditorPanel.ShouldPreviewUseSkybox(); }
    SceneViewMode GetSceneViewMode() const { return m_sceneViewMode; }
    void SetSceneViewMode(SceneViewMode mode) { m_sceneViewMode = mode; }
    float GetSceneGridCellSize() const { return m_sceneGridCellSize; }
    int GetSceneGridHalfLineCount() const { return m_sceneGridHalfLineCount; }
    bool ShouldRenderEffectPreview() const { return m_showEffectEditor && m_activeWorkspace == WorkspaceTab::EffectEditor; }
    DirectX::XMFLOAT2 GetEffectPreviewRenderSize() const { return m_effectEditorPanel.GetPreviewRenderSize(); }
    DirectX::XMFLOAT3 GetEffectPreviewCameraPosition() const { return m_effectEditorPanel.GetPreviewCameraPosition(); }
    DirectX::XMFLOAT3 GetEffectPreviewCameraTarget() const { return m_effectEditorPanel.GetPreviewCameraTarget(); }
    DirectX::XMFLOAT3 GetEffectPreviewCameraDirection() const { return m_effectEditorPanel.GetPreviewCameraDirection(); }
    float GetEffectPreviewCameraFovY() const { return m_effectEditorPanel.GetPreviewCameraFovY(); }
    float GetEffectPreviewNearZ() const { return m_effectEditorPanel.GetPreviewNearZ(); }
    float GetEffectPreviewFarZ() const { return m_effectEditorPanel.GetPreviewFarZ(); }
    DirectX::XMFLOAT4 GetEffectPreviewClearColor() const { return m_effectEditorPanel.GetPreviewClearColor(); }
    bool ShouldEffectPreviewUseSkybox() const { return m_effectEditorPanel.ShouldPreviewUseSkybox(); }
    bool ShouldRenderGameView2DUIOverlay() const { return m_sceneViewMode == SceneViewMode::Mode2D && m_gameViewShowUIOverlay; }
    bool TryBuildGameView2DPreviewViewProjection(DirectX::XMFLOAT4X4& outView,
                                                 DirectX::XMFLOAT4X4& outProjection) const;
    void SetGBufferDebugTextures(ITexture* g0, ITexture* g1, ITexture* g2, ITexture* g3, ITexture* depth) {
        m_gbufferTexture0 = g0;
        m_gbufferTexture1 = g1;
        m_gbufferTexture2 = g2;
        m_gbufferTexture3 = g3;
        m_gbufferDepthTexture = depth;
    }

    // 現在の scene path。GameLoop の Play / Stop 時に editor scene を復元するために使う。
    const std::string& GetCurrentScenePath() const { return m_sceneSavePath; }

    // 入力データを読み込む。 a scene from a path (publicly exposed for EngineKernel::Stop).
    bool LoadSceneFromPath(const std::filesystem::path& scenePath);

    // AI automation から現在シーンを保存する。空 path の場合は現在の保存先を使う。
    bool SaveSceneFromAutomation(const std::filesystem::path& scenePath = {});
    void CheckRecoveryCandidateFromAutomation();
    bool HasRecoveryCandidateForAutomation() const { return !m_pendingRecoveryAutosavePath.empty(); }
    const std::filesystem::path& GetRecoveryAutosavePathForAutomation() const { return m_pendingRecoveryAutosavePath; }
    const std::filesystem::path& GetRecoveryScenePathForAutomation() const { return m_pendingRecoveryScenePath; }
    bool RecoverAutosaveFromAutomation();
    bool DismissAutosaveRecoveryFromAutomation();
    bool OpenEffectEditorFromAutomation(const std::filesystem::path& effectGraphPath = {});
    bool PlayEffectTimelineFromAutomation(const std::filesystem::path& effectGraphPath = {}, float startTime = 0.0f, bool paused = false);
    bool StopEffectTimelineFromAutomation();
    EntityID GetEffectPreviewEntity() const { return m_effectEditorPanel.GetPreviewEntity(); }
    bool IsEffectEditorWorkspaceActive() const { return m_showEffectEditor && m_activeWorkspace == WorkspaceTab::EffectEditor; }
    const std::string& GetEffectEditorDocumentPath() const { return m_effectEditorPanel.GetDocumentPath(); }
    DirectX::XMFLOAT4 GetEffectEditorRect() const { return m_effectEditorPanel.GetWorkspaceRectForAutomation(); }
    DirectX::XMFLOAT4 GetEffectPreviewRect() const { return m_effectEditorPanel.GetPreviewRectForAutomation(); }
    EffectEditorPanel& GetEffectEditorPanel() { return m_effectEditorPanel; }
    const EffectEditorPanel& GetEffectEditorPanel() const { return m_effectEditorPanel; }

    bool OpenPlayerEditorFromAutomation(const std::filesystem::path& modelPath = {});

    GameLoopEditorPanel& GetGameLoopEditorPanel() { return m_gameLoopEditorPanel; }
    const GameLoopEditorPanel& GetGameLoopEditorPanel() const { return m_gameLoopEditorPanel; }
    bool IsGameLoopEditorActive() const { return m_showGameLoopEditor; }
    bool OpenGameLoopEditorFromAutomation(const std::filesystem::path& assetPath = {});

    TerrainEditorPanel& GetTerrainEditorPanel() { return m_terrainEditorPanel; }
    const TerrainEditorPanel& GetTerrainEditorPanel() const { return m_terrainEditorPanel; }
    bool IsTerrainEditorActive() const { return m_showTerrainEditor; }
    bool OpenTerrainEditorFromAutomation(EntityID entity = Entity::NULL_ID);

    // ---- Sequencer Automation ----
    SequencerPanel& GetSequencerPanel() { return m_sequencerPanel; }
    const SequencerPanel& GetSequencerPanel() const { return m_sequencerPanel; }
    bool IsSequencerActive() const { return m_showSequencer; }
    bool OpenSequencerFromAutomation();

    // ---- UI Editor Automation ----
    UIEditorPanel& GetUIEditorPanel() { return m_uiEditorPanel; }
    const UIEditorPanel& GetUIEditorPanel() const { return m_uiEditorPanel; }
    bool IsUIEditorActive() const { return m_showUIEditor && m_activeWorkspace == WorkspaceTab::UIEditor; }
    bool OpenUIEditorFromAutomation();

    // ---- Lighting Automation ----
    bool OpenLightingWindowFromAutomation();

    // ---- Scene View Automation ----
    SceneShadingMode GetSceneShadingMode()     const { return m_sceneShadingMode; }
    void SetSceneShadingModeAutomation(SceneShadingMode m)   { m_sceneShadingMode = m; }
    GizmoOperation GetGizmoOperation()         const { return m_gizmoOperation; }
    void SetGizmoOperationAutomation(GizmoOperation op)      { m_gizmoOperation = op; }
    GizmoSpace GetGizmoSpace()                 const { return m_gizmoSpace; }
    void SetGizmoSpaceAutomation(GizmoSpace s)               { m_gizmoSpace = s; }
    float GetEditorCameraYaw()                 const { return m_editorCameraYaw; }
    float GetEditorCameraPitch()               const { return m_editorCameraPitch; }
    float GetCameraMoveSpeed()                 const { return m_cameraMoveSpeed; }
    void  SetCameraMoveSpeedAutomation(float s)              { m_cameraMoveSpeed = s; }
    void  SetEditorCameraTransformAutomation(const DirectX::XMFLOAT3& pos, float yaw, float pitch)
    {
        m_editorCameraPosition   = pos;
        m_editorCameraYaw        = yaw;
        m_editorCameraPitch      = pitch;
        m_editorCameraUserOverride = true;
    }
    void SaveCameraBookmarkAutomation(int slot) { if (slot >= 0 && slot < 3) SaveCameraBookmark((size_t)slot); }
    void LoadCameraBookmarkAutomation(int slot) { if (slot >= 0 && slot < 3) LoadCameraBookmark((size_t)slot); }

    bool GetShowSceneGizmo()          const { return m_showSceneGizmo; }
    void SetShowSceneGizmo(bool v)          { m_showSceneGizmo = v; }
    bool GetShowSceneStats()          const { return m_showSceneStatsOverlay; }
    void SetShowSceneStats(bool v)          { m_showSceneStatsOverlay = v; }
    bool GetShowSelectionOutline()    const { return m_showSceneSelectionOutline; }
    void SetShowSelectionOutline(bool v)    { m_showSceneSelectionOutline = v; }
    bool GetShowLightIcons()          const { return m_showSceneLightIcons; }
    void SetShowLightIcons(bool v)          { m_showSceneLightIcons = v; }
    bool GetShowCameraIcons()         const { return m_showSceneCameraIcons; }
    void SetShowCameraIcons(bool v)         { m_showSceneCameraIcons = v; }
    bool GetShowBounds()              const { return m_showSceneBounds; }
    void SetShowBounds(bool v)              { m_showSceneBounds = v; }
    bool GetShowCollision()           const { return m_showSceneCollision; }
    void SetShowCollision(bool v)           { m_showSceneCollision = v; }
    bool GetShowInputDebug()          const { return m_showInputDebug; }
    void SetShowInputDebug(bool v)          { m_showInputDebug = v; }

    // ---- Panel Visibility Automation ----
    bool GetShowSceneView()           const { return m_showSceneView; }
    void SetShowSceneView(bool v)           { m_showSceneView = v; }
    bool GetShowGameView()            const { return m_showGameView; }
    void SetShowGameView(bool v)            { m_showGameView = v; }
    bool GetShowHierarchy()           const { return m_showHierarchy; }
    void SetShowHierarchy(bool v)           { m_showHierarchy = v; }
    bool GetShowInspector()           const { return m_showInspector; }
    void SetShowInspector(bool v)           { m_showInspector = v; }
    bool GetShowAssetBrowser()        const { return m_showAssetBrowser; }
    void SetShowAssetBrowser(bool v)        { m_showAssetBrowser = v; }
    bool GetShowConsole()             const { return m_showConsole; }
    void SetShowConsole(bool v)             { m_showConsole = v; }
    bool GetShowSerializer()          const { return m_showSerializer; }
    void SetShowSerializer(bool v)          { m_showSerializer = v; }
    bool GetShowAudioWindow()         const { return m_showAudioWindow; }
    void SetShowAudioWindow(bool v)         { m_showAudioWindow = v; }
    bool GetShowRenderPasses()        const { return m_showRenderPassesWindow; }
    void SetShowRenderPasses(bool v)        { m_showRenderPassesWindow = v; }
    bool GetShowGBufferDebug()        const { return m_showGBufferDebug; }
    void SetShowGBufferDebug(bool v)        { m_showGBufferDebug = v; }
    bool GetShowGridSettings()        const { return m_showGridSettingsWindow; }
    void SetShowGridSettings(bool v)        { m_showGridSettingsWindow = v; }
    bool GetShowStatusBar()           const { return m_showStatusBar; }
    void SetShowStatusBar(bool v)           { m_showStatusBar = v; }

    // ---- Scene Grid Automation ----
    void SetSceneGridCellSize(float v)      { m_sceneGridCellSize = v; }
    void SetSceneGridHalfLineCount(int v)   { m_sceneGridHalfLineCount = v; }

    // ---- 2D View Automation ----
    DirectX::XMFLOAT2 Get2DCenter()   const { return m_editor2DCenter; }
    float             Get2DZoom()     const { return m_editor2DZoom; }
    void Set2DCenter(const DirectX::XMFLOAT2& c) { m_editor2DCenter = c; }
    void Set2DZoom(float z)                 { m_editor2DZoom = z; }

    // ---- Snap Settings Automation ----
    bool  GetTranslateSnapEnabled()   const { return m_translateSnapEnabled; }
    void  SetTranslateSnapEnabled(bool v)   { m_translateSnapEnabled = v; }
    bool  GetRotateSnapEnabled()      const { return m_rotateSnapEnabled; }
    void  SetRotateSnapEnabled(bool v)      { m_rotateSnapEnabled = v; }
    bool  GetScaleSnapEnabled()       const { return m_scaleSnapEnabled; }
    void  SetScaleSnapEnabled(bool v)       { m_scaleSnapEnabled = v; }
    float GetTranslateSnapStep()      const { return m_translateSnapStep; }
    void  SetTranslateSnapStep(float v)     { m_translateSnapStep = v; }
    float GetRotateSnapStep()         const { return m_rotateSnapStep; }
    void  SetRotateSnapStep(float v)        { m_rotateSnapStep = v; }
    float GetScaleSnapStep()          const { return m_scaleSnapStep; }
    void  SetScaleSnapStep(float v)         { m_scaleSnapStep = v; }

    // ---- Game View Settings Automation ----
    GameViewResolutionPreset GetGameViewResolutionPreset() const { return m_gameViewResolutionPreset; }
    void SetGameViewResolutionPreset(GameViewResolutionPreset p) { m_gameViewResolutionPreset = p; }
    GameViewAspectPolicy GetGameViewAspectPolicy()         const { return m_gameViewAspectPolicy; }
    void SetGameViewAspectPolicy(GameViewAspectPolicy p)         { m_gameViewAspectPolicy = p; }
    GameViewScalePolicy  GetGameViewScalePolicy()          const { return m_gameViewScalePolicy; }
    void SetGameViewScalePolicy(GameViewScalePolicy p)           { m_gameViewScalePolicy = p; }
    bool GetGameViewShowSafeArea()    const { return m_gameViewShowSafeArea; }
    void SetGameViewShowSafeArea(bool v)    { m_gameViewShowSafeArea = v; }
    bool GetGameViewShowStats()       const { return m_gameViewShowStatsOverlay; }
    void SetGameViewShowStats(bool v)       { m_gameViewShowStatsOverlay = v; }
    bool GetGameViewShowUIOverlay()   const { return m_gameViewShowUIOverlay; }
    void SetGameViewShowUIOverlay(bool v)   { m_gameViewShowUIOverlay = v; }
    bool GetGameViewShow2DOverlay()   const { return m_gameViewShow2DOverlay; }
    void SetGameViewShow2DOverlay(bool v)   { m_gameViewShow2DOverlay = v; }

    // ---- Undo / Redo Automation ----
    void ExecuteUndoAutomation()            { ExecuteUndo(); }
    void ExecuteRedoAutomation()            { ExecuteRedo(); }

    // ---- Window Focus Automation ----
    void FocusPanelAutomation(WindowFocusTarget t) { RequestWindowFocus(t); }

    // ---- Camera Align Automation ----
    void AlignCameraToViewAutomation()      { AlignMainCameraEntityToEditorCamera(); }

    // ---- New Scene Automation ----
    void RequestNewSceneAutomation(SceneViewMode mode = SceneViewMode::Mode3D)
    {
        m_pendingNewSceneRequest = true;
        m_pendingNewSceneMode    = mode;
    }

private:
    GameLayer* m_gameLayer;
    std::unique_ptr<AssetBrowser> m_assetBrowser;
    ModelSerializerPanel m_modelSerializerPanel;
    EditorInputBridge m_inputBridge;

    EntityID m_selectedEntity = Entity::NULL_ID;
    bool m_showSceneView = true;
    bool m_showGameView = true;
    bool m_showHierarchy = true;
    bool m_showInspector = true;
    bool m_showAssetBrowser = true;
    bool m_showSerializer = false;
    bool m_showConsole = true;
    bool m_showSequencer = false;
    bool m_showLightingWindow = false;
    bool m_showAudioWindow = false;
    bool m_showRenderPassesWindow = false;
    bool m_showGridSettingsWindow = false;
    bool m_showGBufferDebug = false;
    bool m_showStatusBar = true;
    bool m_showMainToolbar = true;
    bool m_showSceneGrid3D = true;
    bool m_showSceneGrid2D = false;
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
    bool m_showEffectEditor = false;
    bool m_showUIEditor = false;
    bool m_showGameLoopEditor = false;
    bool m_showTerrainEditor  = false;
    TerrainEditorPanel m_terrainEditorPanel;
    CreateTerrainDialog m_createTerrainDialog;
    WorkspaceTab m_activeWorkspace = WorkspaceTab::LevelEditor;
    PlayerEditorPanel m_playerEditorPanel;
    EffectEditorPanel m_effectEditorPanel;
    UIEditorPanel m_uiEditorPanel;
    SequencerPanel m_sequencerPanel;
    GameLoopEditorPanel m_gameLoopEditorPanel;
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
    std::filesystem::path m_pendingRecoveredSceneSavePath;
    bool m_hasCheckedRecovery = false;
    double m_autosaveAccumulator = 0.0;

    // PlayerEditor から editor camera に加算する一時 shake オフセット。
    DirectX::XMFLOAT3 m_editorCameraShakeOffset = { 0.0f, 0.0f, 0.0f };
    void DrawDockSpace();
    void DrawWorkspaceTabs();
    void DrawMenuBar();
    void DrawMainToolbar();
    void DrawPlayerEditorWorkspace();
    void DrawEffectEditorWorkspace();
    void DrawUIEditorWorkspace();
    void DrawSequencer();
    void DrawSceneView();
    void DrawGameView();
    void DrawHierarchy();
    void DrawInspector();
    void DrawModelSerializer();
    void DrawLightingWindow();
    void DrawAudioWindow();
    void DrawRenderPassesWindow();
    void DrawGridSettingsWindow();
    void DrawGBufferDebugWindow();
    void DrawTerrainEditor();
    void DrawStatusBar();
    void DrawUnsavedChangesPopup();
    void DrawRecoveryPopup();
    void SyncPlayerEditorPanelState();
    void SyncEffectEditorPanelState();
    void HandleEditorShortcuts();
    void DrawSceneViewToolbar();
    void DrawTransformGizmo();
    void HandleTerrainBrushStroke();
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
    void NewScene(SceneViewMode mode);
    void DrawNewSceneModePopup();
    bool OpenScene();
    bool SaveCurrentScene();
    bool SaveCurrentSceneAs();

    bool m_requestNewScene = false;
    bool m_requestOpenScene = false;
    bool m_requestSaveSceneAs = false;
    bool m_openNewSceneModePopup = false;
    SceneViewMode m_newSceneSelectedMode = SceneViewMode::Mode3D;
    // NewScene 実行は frame 間の Update へ遅延する。RenderUI 中に呼ぶと、
    // GPU resource を持つ entity を破棄してしまい、
    // in-flight frame の command list に記録済みの参照が残る。
    bool m_pendingNewSceneRequest = false;
    SceneViewMode m_pendingNewSceneMode = SceneViewMode::Mode3D;
    GameViewResolutionPreset m_gameViewResolutionPreset = GameViewResolutionPreset::Free;
    GameViewAspectPolicy m_gameViewAspectPolicy = GameViewAspectPolicy::Fit;
    GameViewScalePolicy m_gameViewScalePolicy = GameViewScalePolicy::AutoFit;
    bool m_gameViewShowSafeArea = false;
    bool m_gameViewShowPixelPreview = false;
    bool m_gameViewShowStatsOverlay = false;
    bool m_gameViewShowUIOverlay = true;
    bool m_gameViewShow2DOverlay = true;
    bool m_gameViewUseSceneViewCameraFallback2D = false;
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
    void AlignMainCameraEntityToEditorCamera();
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
    void DrawSequencerCameraOverlay(const DirectX::XMFLOAT4& viewRect,
                                    const DirectX::XMFLOAT4X4& view,
                                    const DirectX::XMFLOAT4X4& projection) const;
    void DrawSceneBoundsOverlay(const DirectX::XMFLOAT4& viewRect,
                                const DirectX::XMFLOAT4X4& view,
                                const DirectX::XMFLOAT4X4& projection) const;
    void DrawSceneCollisionOverlay(const DirectX::XMFLOAT4& viewRect,
                                   const DirectX::XMFLOAT4X4& view,
                                   const DirectX::XMFLOAT4X4& projection) const;
};
