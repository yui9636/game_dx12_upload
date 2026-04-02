#pragma once
#include <string>
#include <memory>
#include <vector>
#include "TimelineAsset.h"
#include "StateMachineAsset.h"
#include "PreviewState.h"
#include "InputMappingTab.h"
#include "Component/NodeAttachComponent.h"

struct ImVec2;
class Registry;
class ITexture;
class Model;

// ============================================================================
// Player Editor — UE-style multi-panel DockSpace editor
// 7 docked sub-windows:
//   Left:   Skeleton Tree / StateMachine (tabbed)
//   Center: 3D Viewport (large)
//   Right:  Properties (top) / Animator+Input (bottom, tabbed)
//   Bottom: Timeline (full width)
// ============================================================================

class PlayerEditorPanel
{
public:
    void Draw(Registry* registry, bool* p_open, bool* outFocused);
    void DrawWorkspace(Registry* registry, bool* outFocused);
    void DrawDetached(Registry* registry, bool* p_open, bool* outFocused);

    // Viewport texture (set by EditorLayer/Renderer)
    void SetViewportTexture(ITexture* tex) { m_viewportTexture = tex; }

    // Model for bone tree (set externally from selected entity)
    void SetModel(const Model* model);

    // Asset access
    TimelineAsset&       GetTimelineAsset()       { return m_timelineAsset; }
    StateMachineAsset&   GetStateMachineAsset()   { return m_stateMachineAsset; }
    PreviewState&        GetPreviewState()        { return m_previewState; }

    // Selected bone (used by timeline item inspector)
    int  GetSelectedBoneIndex() const { return m_selectedBoneIndex; }

private:
    enum class HostMode
    {
        Window,
        Workspace,
        Detached
    };

    void DrawInternal(Registry* registry, bool* p_open, bool* outFocused, HostMode hostMode);

    // ── DockSpace Layout ──
    void BuildDockLayout(unsigned int dockspaceId);
    bool m_needsLayoutRebuild = true;
    HostMode m_lastHostMode = HostMode::Window;

    // ── Sub-windows ──
    void DrawViewportPanel();
    void DrawSkeletonPanel();       // Bone tree + Sockets
    void DrawStateMachinePanel();
    void DrawTimelinePanel();
    void DrawPropertiesPanel();
    void DrawAnimatorPanel();
    void DrawInputPanel();

    // ── Skeleton internals ──
    void DrawBoneTreeNode(int nodeIndex);
    void DrawSocketList();

    // ── Timeline internals ──
    void DrawTimelineTrackHeaders(float height);
    void DrawTimelineGrid(float height);
    void DrawTimelinePlaybackToolbar();
    void DrawTimelineItemInspector();

    // ── StateMachine internals ──
    void DrawNodeGraph(ImVec2 canvasSize);
    void DrawStateNodeInspector();
    void DrawTransitionConditionEditor(struct StateTransition* trans);

    // ── Connection mode (drag-wire) ──
    bool     m_isConnecting      = false;
    uint32_t m_connectFromNodeId = 0;

    // ── Selection context for Properties panel ──
    enum class SelectionContext { None, StateNode, Transition, TimelineTrack, TimelineItem, Bone, Socket };
    SelectionContext m_selectionCtx = SelectionContext::None;

    // ── Assets ──
    TimelineAsset       m_timelineAsset;
    StateMachineAsset   m_stateMachineAsset;

    // ── Preview ──
    PreviewState m_previewState;

    // ── Input mapping ──
    InputMappingTab m_inputMappingTab;

    // ── Timeline state ──
    int   m_playheadFrame    = 0;
    bool  m_isPlaying        = false;
    float m_timelineZoom     = 1.0f;
    float m_timelineScrollX  = 0.0f;
    int   m_selectedTrackId  = -1;
    int   m_selectedItemIdx  = -1;

    // ── StateMachine state ──
    uint32_t m_selectedNodeId       = 0;
    uint32_t m_selectedTransitionId = 0;
    DirectX::XMFLOAT2 m_graphOffset = { 200, 150 };
    float    m_graphZoom            = 1.0f;

    // ── Skeleton state ──
    const Model* m_model             = nullptr;
    int          m_selectedBoneIndex  = -1;
    std::string  m_selectedBoneName;
    char         m_boneSearchFilter[128] = {};
    std::vector<NodeSocket> m_sockets;         // Editable socket list
    int          m_selectedSocketIdx  = -1;

    // ── Animator state ──
    int m_selectedAnimIndex = -1;
    std::string m_currentModelPath;

    // ── Viewport ──
    ITexture* m_viewportTexture = nullptr;
    float m_vpCameraYaw   = 0.0f;
    float m_vpCameraPitch = 0.2f;
    float m_vpCameraDist  = 5.0f;

    // ── Per-frame ──
    Registry* m_registry = nullptr;
};
