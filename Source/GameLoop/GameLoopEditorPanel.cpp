#include "GameLoopEditorPanel.h"

#include "GameLoopEditorPanelInternal.h"


GameLoopEditorPanel::GameLoopEditorPanel()
    : m_internal(std::make_unique<GameLoopEditorPanelInternal>())
{
}


GameLoopEditorPanel::~GameLoopEditorPanel() = default;


void GameLoopEditorPanel::Draw(bool* p_open, bool* outFocused)
{
    m_internal->Draw(p_open, outFocused);
}


const std::filesystem::path& GameLoopEditorPanel::GetCurrentPath() const
{
    return m_internal->GetCurrentPath();
}


void GameLoopEditorPanel::SetCurrentPath(const std::filesystem::path& p)
{
    m_internal->SetCurrentPath(p);
}
