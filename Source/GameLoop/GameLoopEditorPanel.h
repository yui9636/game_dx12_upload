#pragma once

#include <filesystem>
#include <string>

#include "GameLoopAsset.h"

// GameLoopAsset を編集するための ImGui パネル。
// list 式 UI で node / transition / condition を編集する。
// ノードグラフは将来 (v2+) で扱う。
class GameLoopEditorPanel
{
public:
    GameLoopEditorPanel();

    // ImGui 上にパネルを描画する。p_open は閉じるボタン連動、outFocused は最後にフォーカス取得した可否。
    void Draw(bool* p_open = nullptr, bool* outFocused = nullptr);

    // 現在開いている asset path。Save 時に書き込まれる。
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

    // Default save path.
    std::filesystem::path m_currentPath = "Data/GameLoop/Main.gameloop";

    int  m_selectedNodeIndex       = -1;
    int  m_selectedTransitionIndex = -1;

    GameLoopValidateResult m_lastValidate;
    bool                   m_validatedOnce = false;

    char m_loadPathBuf[512] = {};
    char m_saveAsPathBuf[512] = {};
};
