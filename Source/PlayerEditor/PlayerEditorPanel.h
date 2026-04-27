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

// ============================================================================
// Player Editor 窶・UE-style multi-panel DockSpace editor
// 7 docked sub-windows:
//   Left:   Skeleton Tree / StateMachine (tabbed)
//   Center: 3D Viewport (large)
//   Right:  Properties (top) / Animator+Input (bottom, tabbed)
//   Bottom: Timeline (full width)
// ============================================================================

// v2.0 ActorEditor: PlayerEditor edits one of three actor kinds.
// See Docs/ActorEditor_StateBoundBT_Spec_v2.0_2026-04-27.md.
enum class ActorEditorMode : uint8_t
{
    Player = 0,
    Enemy  = 1,
    NPC    = 2,
};

class PlayerEditorPanel
{
public:
    void Draw(Registry* registry, bool* p_open, bool* outFocused);
    void DrawWorkspace(Registry* registry, bool* outFocused);
    void DrawDetached(Registry* registry, bool* p_open, bool* outFocused);
    void Suspend();

    // Viewport texture (set by EditorLayer/Renderer)
    void SetViewportTexture(ITexture* tex) { m_viewportTexture = tex; }
    void SetSharedSceneCamera(const DirectX::XMFLOAT3& position, const DirectX::XMFLOAT3& direction, float fovY);
    bool CanRenderPreview() const { return HasOpenModel(); }
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
    float GetPreviewNearZ() const { return 0.03f; }
    float GetPreviewFarZ() const { return 500.0f; }
    DirectX::XMFLOAT4 GetPreviewClearColor() const { return { 0.12f, 0.12f, 0.12f, 1.0f }; }
    bool ShouldPreviewUseSkybox() const { return false; }
    const Model* GetPreviewModel() const { return m_model; }
    float GetPreviewModelScale() const { return m_previewModelScale; }

    // Model for bone tree (legacy external sync path)
    void SetModel(const Model* model);
    void SetPreviewEntity(EntityID entity);
    void SyncExternalSelection(EntityID entity, const std::string& modelPath);

    // Asset access
    TimelineAsset&       GetTimelineAsset()       { return m_timelineAsset; }
    StateMachineAsset&   GetStateMachineAsset()   { return m_stateMachineAsset; }
    PreviewState&        GetPreviewState()        { return m_previewState; }

    // Selected bone (used by timeline item inspector)
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
    void DrawToolbar();
    bool DrawToolbarButton(const char* label, bool enabled = true);
    void DrawEmptyState();
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

    // 笏笏 DockSpace Layout 笏笏
    void BuildDockLayout(unsigned int dockspaceId);
    bool m_needsLayoutRebuild = true;
    HostMode m_lastHostMode = HostMode::Window;

    // 笏笏 Sub-windows 笏笏
    void DrawViewportPanel();
    void DrawSkeletonPanel();       // Bone tree + Sockets
    void DrawStateMachinePanel();
    void DrawTimelinePanel();
    void DrawPropertiesPanel();
    void DrawAnimatorPanel();
    void DrawInputPanel();

    // 笏笏 Skeleton internals 笏笏
    void DrawBoneTreeNode(int nodeIndex);
    void DrawSocketList(float height);

    // 笏笏 Timeline internals 笏笏
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

    // 笏笏 StateMachine internals 笏笏
    void DrawNodeGraph(ImVec2 canvasSize);
    void FitGraphToContent(const ImVec2& canvasSize);
    void DrawStateNodeInspector();
    // v2.0 state-bound BT integration into State Inspector (Enemy / NPC mode only).
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

    // 笏笏 Connection mode (drag-wire) 笏笏
    bool     m_isConnecting      = false;
    uint32_t m_connectFromNodeId = 0;

    // 笏笏 Selection context for Properties panel 笏笏
    enum class SelectionContext { None, StateNode, Transition, TimelineTrack, TimelineItem, Bone, Socket, PersistentCollider };
    SelectionContext m_selectionCtx = SelectionContext::None;

    // 笏笏 Assets 笏笏
    TimelineAsset       m_timelineAsset;
    StateMachineAsset   m_stateMachineAsset;
    bool                m_timelineDirty = false;
    bool                m_stateMachineDirty = false;
    bool                m_socketDirty = false;
    bool                m_colliderDirty = false;

    // v2.0: ActorEditor mode (Player / Enemy / NPC). Affects toolbar buttons
    // and the StateMachinePanel AI section visibility.
    ActorEditorMode     m_actorEditorMode = ActorEditorMode::Player;

    // v2.0: state-bound BT inline editor state.
    BehaviorTreeAsset   m_inlineBtAsset;
    uint32_t            m_inlineBtStateId        = 0;     // which StateNode owns the loaded asset
    int                 m_inlineBtSelectedIndex  = -1;
    bool                m_inlineBtLoaded         = false; // false = empty / unsynced
    bool                m_inlineBtDirty          = false;
    BTValidateResult    m_inlineBtLastValidate;
    bool                m_inlineBtValidatedOnce  = false;
    bool                m_inlineBtExpanded       = false;

    // 笏笏 Preview 笏笏
    PreviewState m_previewState;

    // 笏笏 Input mapping 笏笏
    InputMappingTab m_inputMappingTab;

    // 笏笏 Timeline state 笏笏
    int   m_playheadFrame    = 0;
    bool  m_isPlaying        = false;
    float m_timelineZoom     = 1.0f;
    float m_timelineScrollX  = 0.0f;
    int   m_selectedTrackId  = -1;
    int   m_selectedItemIdx  = -1;

    // 笏笏 StateMachine state 笏笏
    uint32_t m_selectedNodeId       = 0;
    uint32_t m_selectedTransitionId = 0;
    DirectX::XMFLOAT2 m_graphOffset = { 200, 150 };
    float    m_graphZoom            = 1.0f;
    bool     m_graphRightPanActive    = false;
    DirectX::XMFLOAT2 m_graphRightPanStart = { 0.0f, 0.0f };
    bool     m_graphConnectDragActive = false;
    uint32_t m_graphConnectDragFrom   = 0;
    bool     m_graphFitRequested      = true;

    // 笏笏 Skeleton state 笏笏
    std::shared_ptr<Model> m_ownedModel;
    const Model* m_model             = nullptr;
    int          m_selectedBoneIndex  = -1;
    int          m_hoveredBoneIndex   = -1;
    std::string  m_selectedBoneName;
    char         m_boneSearchFilter[128] = {};
    std::vector<NodeSocket> m_sockets;         // Editable socket list
    int          m_selectedSocketIdx  = -1;
    int          m_selectedColliderIdx = -1;

    // 笏笏 Animator state 笏笏
    int m_selectedAnimIndex = -1;
    std::string m_currentModelPath;
    EntityID m_selectedEntity = Entity::NULL_ID;
    std::string m_selectedEntityModelPath;
    EntityID m_previewEntity = Entity::NULL_ID;
    bool m_previewEntityOwned = false;
    uint32_t m_runtimeObservedStateId = 0;
    uint32_t m_runtimePreviousStateId = 0;
    std::string m_runtimeLastTransitionLabel;

    // 笏笏 Viewport 笏笏
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
    float m_previewModelScale = 1.0f;
    bool m_hasPendingCameraFit = false;
    DirectX::XMFLOAT3 m_pendingCameraFitTarget = { 0.0f, 0.0f, 0.0f };
    float m_pendingCameraFitRadius = 1.0f;
    DirectX::XMFLOAT3 m_pendingCameraFitForward = { 0.0f, 0.0f, 1.0f };
    float m_pendingCameraFitDistance = 0.0f;

    // 笏笏 Per-frame 笏笏
    Registry* m_registry = nullptr;
};
