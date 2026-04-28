#pragma once
#include <filesystem>
#include <string>
#include <DirectXMath.h>

#include "GameLoopAsset.h"

struct ImVec2;

// ImGui panel for editing GameLoopAsset as a scene-flow node graph.
class GameLoopEditorPanel
{
public:
    GameLoopEditorPanel();

    // Draws the whole GameLoop editor window.
    void Draw(bool* p_open = nullptr, bool* outFocused = nullptr);

    const std::filesystem::path& GetCurrentPath() const { return m_currentPath; }
    void SetCurrentPath(const std::filesystem::path& p) { m_currentPath = p; }

private:
    // Draws top-level command buttons and file popups.
    void DrawToolbar();

    // Draws the left side selection helper and validation summary.
    void DrawOutliner();

    // Draws the graph canvas where nodes and transitions are edited visually.
    void DrawGraphCanvas(const ImVec2& size);

    // Draws one scene node and handles node selection / dragging / pin clicks.
    void DrawGraphNode(GameLoopNode& node, const ImVec2& canvasOrigin);

    // Draws one transition edge and handles edge selection.
    void DrawGraphTransition(int transitionIndex, const ImVec2& canvasOrigin);

    // Draws the context menu opened on the graph canvas.
    void DrawGraphContextMenu(const ImVec2& graphMousePosition);

    // Draws the inspector for the selected node / transition / condition.
    void DrawInspector();

    // Draws details for the selected node.
    void DrawNodeInspector(GameLoopNode& node);

    // Draws details for the selected transition.
    void DrawTransitionInspector(GameLoopTransition& transition);

    // Draws the condition summary list for a selected transition.
    void DrawConditionList(GameLoopTransition& transition);

    // Draws the editor for the one selected condition only.
    void DrawSelectedConditionInspector(GameLoopCondition& condition);

    // Draws the latest validate messages.
    void DrawValidateResult();

    // Draws compact runtime status text.
    void DrawRuntimeStatus();

    // Builds a short label for a condition.
    std::string BuildConditionSummary(const GameLoopCondition& condition) const;

    // Builds a short label for an entire transition.
    std::string BuildTransitionSummary(const GameLoopTransition& transition) const;

    // Returns the selected node pointer, or nullptr.
    GameLoopNode* FindSelectedNode();

    // Returns the selected transition pointer, or nullptr.
    GameLoopTransition* FindSelectedTransition();

    // Converts graph-space position to screen-space position.
    ImVec2 GraphToScreen(const DirectX::XMFLOAT2& graphPos, const ImVec2& canvasOrigin) const;

    // Converts screen-space position to graph-space position.
    DirectX::XMFLOAT2 ScreenToGraph(const ImVec2& screenPos, const ImVec2& canvasOrigin) const;

    // Creates a transition with a default Confirm condition.
    void AddTransition(uint32_t fromNodeId, uint32_t toNodeId);

    // Applies the Title -> Battle -> Result -> Battle Confirm test preset.
    void ApplyZTestLoopPreset();

    // Performs validation and caches the result for UI display.
    void DoValidate();

    // Saves to the current path after validation.
    void DoSave();

    // Saves to a new path after validation.
    void DoSaveAs(const std::filesystem::path& newPath);

    // Loads a GameLoop asset from disk.
    void DoLoad(const std::filesystem::path& fromPath);

    // Replaces the current asset with the normal default GameLoop.
    void DoNewDefault();

private:
    std::filesystem::path m_currentPath = "Data/GameLoop/Main.gameloop";

    uint32_t m_selectedNodeId = 0;
    int      m_selectedTransitionIndex = -1;
    int      m_selectedConditionIndex = -1;

    bool     m_isConnecting = false;
    uint32_t m_connectFromNodeId = 0;

    DirectX::XMFLOAT2 m_graphOffset = { 160.0f, 90.0f };
    float             m_graphZoom = 1.0f;

    DirectX::XMFLOAT2 m_contextGraphPosition = { 0.0f, 0.0f };

    GameLoopValidateResult m_lastValidate;
    bool                   m_validatedOnce = false;

    char m_loadPathBuf[512] = {};
    char m_saveAsPathBuf[512] = {};
};
