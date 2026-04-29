#pragma once

#include <filesystem>
#include <string>
#include <DirectXMath.h>

#include "GameLoopAsset.h"

struct ImVec2;

// Dockable, canvas-first editor for GameLoop scene-flow assets.
class GameLoopEditorPanel
{
public:
    GameLoopEditorPanel();

    void Draw(bool* p_open = nullptr, bool* outFocused = nullptr);

    const std::filesystem::path& GetCurrentPath() const { return m_currentPath; }
    void SetCurrentPath(const std::filesystem::path& p) { m_currentPath = p; }

private:
    enum class SelectionContext
    {
        None,
        Node,
        Transition,
        Condition,
        ValidateMessage,
        Runtime,
    };

    enum class NodeTemplate
    {
        Custom,
        Title,
        Battle,
        Result,
    };

    void DrawToolbar();
    void BuildDockLayout(unsigned int dockspaceId);

    void DrawGraphPanel();
    void DrawOutlinerPanel();
    void DrawPropertiesPanel();
    void DrawValidatePanel();
    void DrawRuntimePanel();

    void DrawGraphCanvas(const ImVec2& canvasSize);
    void DrawGrid(const ImVec2& origin, const ImVec2& canvasSize);
    void DrawTransitionEdge(GameLoopTransition& transition, const ImVec2& origin);
    void DrawNode(GameLoopNode& node, const ImVec2& origin);
    void DrawCanvasContextMenu(const ImVec2& origin);
    void DrawNodeContextMenu();
    void DrawTransitionContextMenu();
    void DrawQuickCreatePopup();
    void DrawFloatingTransitionEditor();

    void DrawNodeProperties(GameLoopNode& node);
    void DrawTransitionProperties(GameLoopTransition& transition);
    void DrawLoadingPolicyEditor(GameLoopLoadingPolicy& policy);
    void DrawConditionList(GameLoopTransition& transition);
    void DrawConditionEditor(GameLoopCondition& condition);

    GameLoopNode* FindSelectedNode();
    GameLoopTransition* FindSelectedTransition();
    GameLoopTransition* FindTransitionById(uint32_t transitionId);
    int FindSelectedTransitionIndex() const;

    ImVec2 GraphToScreen(const DirectX::XMFLOAT2& graphPos, const ImVec2& origin) const;
    DirectX::XMFLOAT2 ScreenToGraph(const ImVec2& screenPos, const ImVec2& origin) const;

    void SelectNode(uint32_t nodeId);
    void SelectTransition(uint32_t transitionId);
    void ClearSelection();

    GameLoopNode& AddNodeAt(NodeTemplate nodeTemplate, const DirectX::XMFLOAT2& requestedPosition);
    void AddTransition(uint32_t fromNodeId, uint32_t toNodeId, bool openEditor);
    void ApplyDefaultCondition(GameLoopTransition& transition);
    void AddConditionPreset(GameLoopTransition& transition, int presetIndex);

    void DeleteSelected();
    void DeleteSelectedNode();
    void DeleteSelectedTransition();
    void DuplicateSelected();
    void MoveSelectedTransitionSourceLocal(int direction);
    int GetSourceLocalPriority(const GameLoopTransition& transition) const;

    void RequestFitGraph();
    void FitGraphToContent(const ImVec2& canvasSize);
    void FrameSelected(const ImVec2& canvasSize);
    DirectX::XMFLOAT2 FindOpenGraphPosition(const DirectX::XMFLOAT2& requestedPosition) const;

    void StartNodeNameEdit(const GameLoopNode& node);
    void StartNodePathEdit(const GameLoopNode& node);
    void DrawNodeInlineEditors(GameLoopNode& node, const ImVec2& nodePos, const ImVec2& nodeSize);
    void OpenFloatingTransitionEditor(uint32_t transitionId, const ImVec2& screenPos);

    std::string BuildConditionSummary(const GameLoopCondition& condition) const;
    std::string BuildTransitionLabel(const GameLoopTransition& transition, bool compact) const;
    std::string BuildMiddleEllipsis(const std::string& text, float maxWidth) const;

    void DoValidate();
    void DoSave();
    void DoSaveAs(const std::filesystem::path& newPath);
    void DoLoad(const std::filesystem::path& fromPath);
    void DoNewDefault();
    void ApplyZTestLoopPreset();

private:
    std::filesystem::path m_currentPath = "Data/GameLoop/Main.gameloop";

    SelectionContext m_selection = SelectionContext::None;
    uint32_t m_selectedNodeId = 0;
    uint32_t m_selectedTransitionId = 0;
    int      m_selectedConditionIndex = -1;

    uint32_t m_hoveredNodeId = 0;
    uint32_t m_hoveredTransitionId = 0;
    uint32_t m_hoveredInputNodeId = 0;
    uint32_t m_hoveredOutputNodeId = 0;

    bool     m_isConnecting = false;
    uint32_t m_connectFromNodeId = 0;
    bool     m_connectionDragged = false;
    uint32_t m_quickCreateFromNodeId = 0;
    DirectX::XMFLOAT2 m_quickCreateGraphPos = { 0.0f, 0.0f };

    DirectX::XMFLOAT2 m_graphOffset = { 160.0f, 90.0f };
    float             m_graphZoom = 1.0f;
    bool              m_graphFitRequested = true;
    bool              m_needsLayoutRebuild = true;
    bool              m_rightPanActive = false;
    DirectX::XMFLOAT2 m_rightPanStart = { 0.0f, 0.0f };

    DirectX::XMFLOAT2 m_contextGraphPosition = { 0.0f, 0.0f };
    DirectX::XMFLOAT2 m_floatingEditorScreenPos = { 420.0f, 240.0f };
    uint32_t          m_floatingTransitionId = 0;
    bool              m_floatingTransitionEditorOpen = false;

    uint32_t m_inlineNameNodeId = 0;
    uint32_t m_inlinePathNodeId = 0;
    bool     m_focusInlineEditor = false;
    char     m_inlineNameBuf[128] = {};
    char     m_inlinePathBuf[256] = {};

    GameLoopValidateResult m_lastValidate;
    bool                   m_validatedOnce = false;
    bool                   m_dirty = false;

    char m_loadPathBuf[512] = {};
    char m_saveAsPathBuf[512] = {};
    char m_outlinerSearch[128] = {};
};
