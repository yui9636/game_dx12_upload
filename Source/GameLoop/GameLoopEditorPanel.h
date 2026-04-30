#pragma once

#include <filesystem>
#include <memory>

class GameLoopEditorPanelInternal;


class GameLoopEditorPanel
{
public:
    GameLoopEditorPanel();

    ~GameLoopEditorPanel();

    GameLoopEditorPanel(const GameLoopEditorPanel&) = delete;
    GameLoopEditorPanel& operator=(const GameLoopEditorPanel&) = delete;

    GameLoopEditorPanel(GameLoopEditorPanel&&) = delete;
    GameLoopEditorPanel& operator=(GameLoopEditorPanel&&) = delete;

    void Draw(bool* p_open = nullptr, bool* outFocused = nullptr);

    const std::filesystem::path& GetCurrentPath() const;

    void SetCurrentPath(const std::filesystem::path& p);

private:
    std::unique_ptr<GameLoopEditorPanelInternal> m_internal;
};
