#pragma once
#include <string>
#include <memory>
#include <vector>
#include "TimelineAsset.h"
#include "StateMachineAsset.h"
#include "PreviewState.h"
#include "InputMappingTab.h"
#include "Component/NodeSocket.h"
#include "Entity/Entity.h"

struct ImVec2;
class Registry;
class ITexture;
class Model;

// ============================================================================
// Player Editor 窶・UE-style multi-panel DockSpace editor
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
    void SetPreviewEntity(EntityID entity) { m_previewEntity = entity; }

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
    void DrawSocketList();

    // 笏笏 Timeline internals 笏笏
    void DrawTimelineTrackHeaders(float height);
    void DrawTimelineGrid(float height);
    void DrawTimelinePlaybackToolbar();
    void DrawTimelineItemInspector();

    // 笏笏 StateMachine internals 笏笏
    void DrawNodeGraph(ImVec2 canvasSize);
    void DrawStateNodeInspector();
    void DrawTransitionConditionEditor(struct StateTransition* trans);

    // 笏笏 Connection mode (drag-wire) 笏笏
    bool     m_isConnecting      = false;
    uint32_t m_connectFromNodeId = 0;

    // 笏笏 Selection context for Properties panel 笏笏
    enum class SelectionContext { None, StateNode, Transition, TimelineTrack, TimelineItem, Bone, Socket };
    SelectionContext m_selectionCtx = SelectionContext::None;

    // 笏笏 Assets 笏笏
    TimelineAsset       m_timelineAsset;
    StateMachineAsset   m_stateMachineAsset;

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

    // 笏笏 Skeleton state 笏笏
    const Model* m_model             = nullptr;
    int          m_selectedBoneIndex  = -1;
    std::string  m_selectedBoneName;
    char         m_boneSearchFilter[128] = {};
    std::vector<NodeSocket> m_sockets;         // Editable socket list
    int          m_selectedSocketIdx  = -1;

    // 笏笏 Animator state 笏笏
    int m_selectedAnimIndex = -1;
    std::string m_currentModelPath;
    EntityID m_previewEntity = Entity::NULL_ID;

    // 笏笏 Viewport 笏笏
    ITexture* m_viewportTexture = nullptr;
    float m_vpCameraYaw   = 0.0f;
    float m_vpCameraPitch = 0.2f;
    float m_vpCameraDist  = 5.0f;

    // 笏笏 Per-frame 笏笏
    Registry* m_registry = nullptr;
};
