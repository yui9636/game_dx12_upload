// GameLoopEditorPanel の GameLoop 関連実装をまとめます。
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

// ---- Automation API forwarding ----

GameLoopAsset&       GameLoopEditorPanel::GetAsset()       { return m_internal->GetAsset(); }
const GameLoopAsset& GameLoopEditorPanel::GetAsset() const { return m_internal->GetAsset(); }

bool  GameLoopEditorPanel::IsDirty()    const { return m_internal->IsDirty(); }
void  GameLoopEditorPanel::MarkDirty()        { m_internal->MarkDirtyForAutomation(); }
void  GameLoopEditorPanel::RequestFitGraph()  { m_internal->RequestFitGraph(); }

uint32_t GameLoopEditorPanel::GetSelectedNodeId()       const { return m_internal->GetSelectedNodeId(); }
uint32_t GameLoopEditorPanel::GetSelectedTransitionId() const { return m_internal->GetSelectedTransitionId(); }
void     GameLoopEditorPanel::SelectNodeById(uint32_t id)     { m_internal->SelectNodeById(id); }
void     GameLoopEditorPanel::SelectTransitionById(uint32_t id) { m_internal->SelectTransitionById(id); }
void     GameLoopEditorPanel::ClearSelection()                { m_internal->ClearSelectionAutomation(); }

uint32_t GameLoopEditorPanel::AddSceneNodeAutomation(const std::string& p, const DirectX::XMFLOAT2& pos)
    { return m_internal->AddSceneNodeAutomation(p, pos); }
uint32_t GameLoopEditorPanel::AddFlowNodeAutomation(GameLoopNodeType t, const std::string& name, const DirectX::XMFLOAT2& pos)
    { return m_internal->AddFlowNodeAutomation(t, name, pos); }
uint32_t GameLoopEditorPanel::AddTransitionAutomation(uint32_t from, uint32_t to)
    { return m_internal->AddTransitionAutomation(from, to); }
void GameLoopEditorPanel::DeleteNodeAutomation(uint32_t id)            { m_internal->DeleteNodeAutomation(id); }
void GameLoopEditorPanel::DeleteTransitionByIdAutomation(uint32_t id)  { m_internal->DeleteTransitionByIdAutomation(id); }

void GameLoopEditorPanel::ReverseTransitionByIdAutomation(uint32_t id) { m_internal->ReverseTransitionByIdAutomation(id); }
void GameLoopEditorPanel::ValidateAutomation()                         { m_internal->ValidateAutomation(); }
void GameLoopEditorPanel::SaveAutomation()                             { m_internal->SaveAutomation(); }
void GameLoopEditorPanel::LoadAutomation(const std::filesystem::path& path) { m_internal->LoadAutomation(path); }

const GameLoopValidateResult& GameLoopEditorPanel::GetValidateResult() const { return m_internal->GetValidateResult(); }
const std::string&            GameLoopEditorPanel::GetStatusMessage()  const { return m_internal->GetStatusMessage(); }
