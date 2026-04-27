#pragma once

#include <filesystem>
#include <string>

#include "GameLoopAsset.h"

// ImGui panel for editing GameLoopAsset.
// List-based UI; node graph is left for a future revision.
class GameLoopEditorPanel
{
public:
    GameLoopEditorPanel();

    void Draw(bool* p_open = nullptr, bool* outFocused = nullptr);

    const std::filesystem::path& GetCurrentPath() const { return m_currentPath; }
    void SetCurrentPath(const std::filesystem::path& p) { m_currentPath = p; }

private:
    void DrawToolbar();
    void DrawNodeList();
    void DrawNodeInspector();
    void DrawTransitionList();
    void DrawTransitionInspector();
    void DrawConditionEditor(GameLoopCondition& c, int conditionIndex);
    void DrawValidateResult();
    void DrawRuntimeStatus();

    void DoValidate();
    void DoSave();
    void DoSaveAs(const std::filesystem::path& newPath);
    void DoLoad(const std::filesystem::path& fromPath);
    void DoNewDefault();

    std::filesystem::path m_currentPath = "Data/GameLoop/Main.gameloop";

    int  m_selectedNodeIndex       = -1;
    int  m_selectedTransitionIndex = -1;

    GameLoopValidateResult m_lastValidate;
    bool                   m_validatedOnce = false;

    char m_loadPathBuf[512] = {};
    char m_saveAsPathBuf[512] = {};
};
