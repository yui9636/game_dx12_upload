#pragma once

#include <filesystem>
#include <memory>
// GameLoopEditorPanelInternal はこの機能の公開インターフェースを定義し、実装側が具体的な処理を行う。

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
