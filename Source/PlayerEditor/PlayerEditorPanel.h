#pragma once
#include <string>
#include <memory>
#include <vector>
#include <initializer_list>
#include "TimelineAsset.h"
#include "StateMachineAsset.h"
#include "PreviewState.h"
#include "InputMappingTab.h"
#include "Component/ColliderComponent.h"
#include "Component/NodeSocket.h"
#include "Entity/Entity.h"
#include "AI/BehaviorTreeAsset.h"

struct ImVec2;
class Registry;
class ITexture;
class Model;
class PlayerEditorSession;
// プレイヤーエディター: UE 風のマルチパネル DockSpace エディタ。
// 7 つのドッキング済みサブウィンドウ:
//   左:   スケルトンツリー / ステートマシン（タブ）
//   中央: 3D ビューポート（大）
//   右:   プロパティ（上） / アニメータ + 入力（下、タブ）
//   下:   タイムライン（全幅）
// v2.0 ActorEditor: PlayerEditor は 3 種類のアクターのいずれかを編集する。
// 詳細は ActorEditor_StateBoundBT_Spec_v2.0_2026-04-27.md を参照。
enum class ActorEditorMode : uint8_t
{
    Player = 0,
    Enemy  = 1,
    NPC    = 2,
};

enum class PlayerEditorViewMode : uint8_t
{
    Edit = 0,
    Test = 1,
};

class PlayerEditorPanel
{
public:
    void Draw(Registry* registry, bool* p_open, bool* outFocused);
    void DrawWorkspace(Registry* registry, bool* outFocused);
    void DrawDetached(Registry* registry, bool* p_open, bool* outFocused);
    void Suspend();

    // ビューポートテクスチャ（EditorLayer / Renderer が設定）
    void SetViewportTexture(ITexture* tex) { m_viewportTexture = tex; }
    void SetSharedSceneCamera(const DirectX::XMFLOAT3& position, const DirectX::XMFLOAT3& direction, float fovY);
    bool CanRenderPreview() const { return true; }
    DirectX::XMFLOAT2 GetPreviewRenderSize() const { return m_previewRenderSize; }
    DirectX::XMFLOAT4 GetViewportRect() const { return m_viewportRect; }
    bool IsViewportHovered() const { return m_viewportHovered; }
    bool ConsumePendingCameraFit(
        DirectX::XMFLOAT3& outTarget,
        float& outRadius,
        DirectX::XMFLOAT3* outForward = nullptr,
        float* outDistance = nullptr);
    DirectX::XMFLOAT3 GetPreviewCameraPosition() const;
    DirectX::XMFLOAT3 GetPreviewCameraTarget() const;
    DirectX::XMFLOAT3 GetPreviewCameraDirection() const;
    float GetPreviewCameraFovY() const;
    float GetPreviewNearZ() const { return 0.01f; }
    float GetPreviewFarZ() const { return 5000.0f; }
    // 背景は単色グレー。UE5 Persona の Header 色 (#1F1F1F) 寄り。
    DirectX::XMFLOAT4 GetPreviewClearColor() const { return { 0.122f, 0.122f, 0.122f, 1.0f }; }
    bool ShouldPreviewUseSkybox() const { return false; }
    const Model* GetPreviewModel() const { return m_model; }
    float GetPreviewModelScale() const { return m_previewModelScale; }
    EntityID GetPreviewEntity() const { return m_previewEntity; }
    bool IsTestModeActive() const { return m_viewMode == PlayerEditorViewMode::Test; }

    // ボーンツリー用モデル（旧外部同期経路）
    void SetModel(const Model* model);
    void SetPreviewEntity(EntityID entity);
    void SyncExternalSelection(EntityID entity, const std::string& modelPath);

    // アセットアクセス
    TimelineAsset&       GetTimelineAsset()       { return m_timelineAsset; }
    StateMachineAsset&   GetStateMachineAsset()   { return m_stateMachineAsset; }
    PreviewState&        GetPreviewState()        { return m_previewState; }

    // 選択中ボーン（タイムライン項目インスペクタで使用）
    int  GetSelectedBoneIndex() const { return m_selectedBoneIndex; }

private:
    friend class PlayerEditorSession;

    enum class HostMode
    {
        Window,
        Workspace,
        Detached
    };

    void DrawInternal(Registry* registry, bool* p_open, bool* outFocused, HostMode hostMode);
    void DrawTopBar();
    bool DrawToolbarInlineButton(const char* icon, const char* label, const char* id,
                                  bool active, bool enabled, bool dirtyDot);
    bool DrawToolbarButton(const char* label, bool enabled = true);
    void DrawMainWorkspace();
    void DrawLeftSidebar(float w);
    void DrawWorkspaceTabBar();
    void DrawWorkbench();
    void DrawRightInspector(float w);
    void DrawStatusBar();
    void HandleGlobalShortcuts();
    void DrawViewportSurface();
    void DrawViewportOverlay(const ImVec2& imageMin, const ImVec2& imageSize);
    void RequestCameraFit();
    void ResetPreviewRuntime();
    bool TryBuildThirdPersonPreviewCamera(
        DirectX::XMFLOAT3& outPosition,
        DirectX::XMFLOAT3& outTarget,
        DirectX::XMFLOAT3& outDirection) const;
    void ResetSelectionState();
    bool HasOpenModel() const;
    bool HasAnyDirtyDocument() const;
    bool HasSelectedEntityContext() const;
    bool CanUsePreviewEntity() const;
    bool OpenModelFromPath(const std::string& path);
    bool SavePrefabDocument(bool saveAs);
    void ApplyEditorBindingsToPreviewEntity();
    void EnsureOwnedPreviewEntity();
    void DestroyOwnedPreviewEntity();
    void RebuildPreviewTimelineRuntimeData();
    void SyncPreviewTimelinePlayback(bool syncPreviewState = true);
    void ImportFromSelectedEntity();
    void ImportSocketsFromPreviewEntity();
    void ExportSocketsToPreviewEntity();
    ColliderComponent* GetPreviewColliderComponent(bool createIfMissing);
    bool TryAssignSelectedBoneToPersistentCollider(int boneIndex);
    void SelectPersistentCollider(int colliderIndex);
    void AddPersistentCollider(ColliderAttribute attribute);

    // DockSpace レイアウト
    void BuildDockLayout(unsigned int dockspaceId);
    bool m_needsLayoutRebuild = true;
    HostMode m_lastHostMode = HostMode::Window;

    // サブウィンドウ
    void DrawViewportPanel();
    void DrawSkeletonPanel();       // ボーンツリー + ソケット
    void DrawStateMachinePanel();
    void DrawTimelinePanel();
    void DrawPropertiesPanel();
    void DrawAnimatorPanel();
    void DrawInputPanel();

    // スケルトン内部処理
    void DrawBoneTreeNode(int nodeIndex);
    void DrawSocketList(float height);

    // タイムライン内部処理
    void DrawTimelineTrackHeaders(float height);
    void DrawTimelineGrid(float height);
    void DrawTimelinePlaybackToolbar();
    void DrawTimelineItemInspector();
    void DrawPersistentColliderSection();
    void DrawPersistentColliderInspector();
    bool DrawAnimationSelector(const char* label, int* animIndex);
    bool TryAssignSelectedBoneToTimelineItem(int boneIndex);
    const char* GetBoneNameByIndex(int boneIndex) const;
    int GetTimelineFrameLimit() const;
    float GetTimelinePlaybackDurationSeconds() const;
    float GetSelectedAnimationDurationSeconds() const;
    void StartSelectedAnimationPreview();
    void PreviewStateNode(uint32_t stateId, bool restartTimeline);

    // ステートマシン内部処理
    void DrawNodeGraph(ImVec2 canvasSize);
    void FitGraphToContent(const ImVec2& canvasSize);
    void DrawStateNodeInspector();
    // v2.0: ステートに紐づく BT をステートインスペクタへ統合する（Enemy / NPC モードのみ）。
    void DrawStateAISection(StateNode& state);
    void DrawInlineBTTreeView();
    void DrawInlineBTNodeInspector();
    void LoadInlineBTForState(StateNode& state);
    void SaveInlineBTForState(StateNode& state);
    void GenerateDefaultSubtreeForState(StateNode& state);
    void DrawStateMachineParameterList();
    void DrawStateMachineRuntimeStatus();
    void DrawTransitionConditionEditor(struct StateTransition* trans);
    void AddStateTemplate(StateNodeType type, const DirectX::XMFLOAT2& graphPosition);
    void ApplyFullPlayerPreset();
    void RemoveBrokenTransitions();
    void ApplyLocomotionStateMachinePreset();
    void ApplyAttackComboPreset();
    void ApplyDodgePreset();
    void ApplyDamagePreset();
    void ApplyLocomotionTransitionPreset(struct StateTransition& trans, bool enteringMove);
    void EnsureStateMachineParameter(const char* name, ParameterType type, float defaultValue);
    StateNode* FindStateByName(const char* name);
    struct StateTransition* FindTransition(uint32_t fromState, uint32_t toState);
    int FindAnimationIndexByKeyword(std::initializer_list<const char*> keywords) const;
    int FindIdleAnimation() const;
    int FindWalkAnimation() const;
    int FindJogAnimation() const;
    int FindRunAnimation() const;
    int FindAttackAnimation(int slot) const;
    int FindDodgeAnimation() const;
    int FindDamageAnimation() const;
    bool IsNonForwardLocomotionAnimation(int animationIndex) const;

    // 接続モード（ドラッグワイヤ）
    bool     m_isConnecting      = false;
    uint32_t m_connectFromNodeId = 0;

    // プロパティパネル用の選択コンテキスト
    enum class SelectionContext { None, StateNode, Transition, TimelineTrack, TimelineItem, Bone, Socket, PersistentCollider };
    SelectionContext m_selectionCtx = SelectionContext::None;

    // アセット
    TimelineAsset       m_timelineAsset;
    StateMachineAsset   m_stateMachineAsset;
    bool                m_timelineDirty = false;
    bool                m_stateMachineDirty = false;
    bool                m_socketDirty = false;
    bool                m_colliderDirty = false;

    // v2.0: ActorEditor モード（Player / Enemy / NPC）。ツールバーボタンと
    // StateMachinePanel の AI セクション表示に影響する。
    ActorEditorMode     m_actorEditorMode = ActorEditorMode::Player;

    // UE5 Persona 風 5 領域構成 (TopBar + LeftSidebar + Center{Viewport+Workbench} + RightInspector + StatusBar)
    PlayerEditorViewMode m_viewMode = PlayerEditorViewMode::Edit;
    int                  m_workbenchActiveTab = 0;  // 0=State 1=Attack 2=Hit 3=Input
    bool                 m_workbenchOpen      = true;
    bool                 m_leftSidebarOpen    = true;
    int                  m_autoFitCountdown   = 0;   // モデル読込後 N フレーム自動 Fit
    bool                 m_drawingPipViewport = false; // PiP 描画中フラグ (overlay 抑制用)
    bool                 m_overlayHitboxes = true;
    bool                 m_overlayRuntime = true;

    // v2.0: ステート紐づけ BT のインライン編集状態。
    BehaviorTreeAsset   m_inlineBtAsset;
    uint32_t            m_inlineBtStateId        = 0;     // 読み込み済みアセットを所有する StateNode
    int                 m_inlineBtSelectedIndex  = -1;
    bool                m_inlineBtLoaded         = false; // false なら空または未同期
    bool                m_inlineBtDirty          = false;
    BTValidateResult    m_inlineBtLastValidate;
    bool                m_inlineBtValidatedOnce  = false;
    bool                m_inlineBtExpanded       = false;

    // プレビュー
    PreviewState m_previewState;

    // 入力マッピング
    InputMappingTab m_inputMappingTab;

    // タイムライン状態
    int   m_playheadFrame    = 0;
    bool  m_isPlaying        = false;
    float m_timelineZoom     = 1.0f;
    float m_timelineScrollX  = 0.0f;
    int   m_selectedTrackId  = -1;
    int   m_selectedItemIdx  = -1;

    // ステートマシン状態
    uint32_t m_selectedNodeId       = 0;
    uint32_t m_selectedTransitionId = 0;
    DirectX::XMFLOAT2 m_graphOffset = { 200, 150 };
    float    m_graphZoom            = 1.0f;
    bool     m_graphRightPanActive    = false;
    DirectX::XMFLOAT2 m_graphRightPanStart = { 0.0f, 0.0f };
    bool     m_graphConnectDragActive = false;
    uint32_t m_graphConnectDragFrom   = 0;
    bool     m_graphFitRequested      = true;

    // スケルトン状態
    std::shared_ptr<Model> m_ownedModel;
    const Model* m_model             = nullptr;
    int          m_selectedBoneIndex  = -1;
    int          m_hoveredBoneIndex   = -1;
    std::string  m_selectedBoneName;
    char         m_boneSearchFilter[128] = {};
    std::vector<NodeSocket> m_sockets;         // 編集可能なソケット一覧
    int          m_selectedSocketIdx  = -1;
    int          m_selectedColliderIdx = -1;

    // アニメータ状態
    int m_selectedAnimIndex = -1;
    std::string m_currentModelPath;
    EntityID m_selectedEntity = Entity::NULL_ID;
    std::string m_selectedEntityModelPath;
    EntityID m_previewEntity = Entity::NULL_ID;
    bool m_previewEntityOwned = false;
    DirectX::XMFLOAT3 m_ownedPreviewAuthoringScale = { 1.0f, 1.0f, 1.0f };
    bool m_hasOwnedPreviewAuthoringScale = false;
    uint32_t m_runtimeObservedStateId = 0;
    uint32_t m_runtimePreviousStateId = 0;
    std::string m_runtimeLastTransitionLabel;

    // ビューポート
    ITexture* m_viewportTexture = nullptr;
    DirectX::XMFLOAT2 m_previewRenderSize = { 0.0f, 0.0f };
    DirectX::XMFLOAT4 m_viewportRect = { 0.0f, 0.0f, 0.0f, 0.0f };
    DirectX::XMFLOAT3 m_sharedSceneCameraPosition = { 0.0f, 0.0f, 0.0f };
    DirectX::XMFLOAT3 m_sharedSceneCameraDirection = { 0.0f, 0.0f, 1.0f };
    float m_sharedSceneCameraFovY = 0.785398f;
    bool m_viewportHovered = false;
    float m_vpCameraYaw   = 0.0f;
    float m_vpCameraPitch = 0.05f;
    float m_vpCameraDist  = 5.0f;
    DirectX::XMFLOAT3 m_vpCameraPanOffset = { 0.0f, 0.0f, 0.0f };  // フリーカメラパン用オフセット
    // Fit 時にスナップショットされる軌道中心。アニメ中も追従しない固定ターゲット。
    DirectX::XMFLOAT3 m_orbitCenter       = { 0.0f, 1.0f, 0.0f };
    bool              m_orbitCenterValid  = false;
    float m_previewModelScale = 1.0f;
    bool m_hasPendingCameraFit = false;
    DirectX::XMFLOAT3 m_pendingCameraFitTarget = { 0.0f, 0.0f, 0.0f };
    float m_pendingCameraFitRadius = 1.0f;
    DirectX::XMFLOAT3 m_pendingCameraFitForward = { 0.0f, 0.0f, 1.0f };
    float m_pendingCameraFitDistance = 0.0f;

    // フレーム単位の状態
    Registry* m_registry = nullptr;
};
