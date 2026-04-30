#include "GameLoopEditorPanelInternal.h"

void GameLoopEditorPanelInternal::AddSceneNode(const std::string& path, const DirectX::XMFLOAT2& pos)
{
    std::string norm = GameLoopScenePicker::NormalizeScenePath(path);
    if (norm.empty()) return;

    GameLoopNode n;
    n.id = m_asset.AllocateNodeId();
    n.name = GameLoopScenePicker::BuildNodeNameFromScenePath(norm);
    n.scenePath = norm;
    n.graphPos = pos;

    m_asset.nodes.push_back(n);
    m_nodeViews.push_back({ n.id, pos });

    if (m_asset.startNodeId == 0) m_asset.startNodeId = n.id;

    SelectNode(n.id);
    m_dirty = true;
}

void GameLoopEditorPanelInternal::ReplaceNodeScene(uint32_t id, const std::string& path)
{
    GameLoopNode* n = FindNode(id);
    std::string norm = GameLoopScenePicker::NormalizeScenePath(path);
    if (!n || norm.empty()) return;

    n->scenePath = norm;
    n->name = GameLoopScenePicker::BuildNodeNameFromScenePath(norm);
    m_dirty = true;
}

void GameLoopEditorPanelInternal::AddTransition(uint32_t from, uint32_t to)
{
    if (!FindNode(from) || !FindNode(to) || from == to) return;

    GameLoopTransition t;
    t.id = m_asset.AllocateTransitionId();
    t.fromNodeId = from;
    t.toNodeId = to;
    t.name = "Transition";

    m_asset.transitions.push_back(t);
    SelectTransition((int)m_asset.transitions.size() - 1);
    m_dirty = true;
}

void GameLoopEditorPanelInternal::SelectNode(uint32_t id)
{
    m_selection = SelectionKind::Node;
    m_selectedNodeId = id;
    m_selectedTransitionIndex = -1;
    m_selectedConditionIndex = -1;
}

void GameLoopEditorPanelInternal::SelectTransition(int i)
{
    m_selection = SelectionKind::Transition;
    m_selectedNodeId = 0;
    m_selectedTransitionIndex = i;
    m_selectedConditionIndex = -1;
}

void GameLoopEditorPanelInternal::ClearSelection()
{
    m_selection = SelectionKind::None;
    m_selectedNodeId = 0;
    m_selectedTransitionIndex = -1;
    m_selectedConditionIndex = -1;
}

void GameLoopEditorPanelInternal::DeleteSelected()
{
    if (m_selection == SelectionKind::Node) DeleteNode(m_selectedNodeId);
    else if (m_selection == SelectionKind::Transition) DeleteTransition(m_selectedTransitionIndex);
}

void GameLoopEditorPanelInternal::DeleteNode(uint32_t id)
{
    m_asset.nodes.erase(
        std::remove_if(
            m_asset.nodes.begin(),
            m_asset.nodes.end(),
            [id](const GameLoopNode& n) { return n.id == id; }),
        m_asset.nodes.end());

    m_asset.transitions.erase(
        std::remove_if(
            m_asset.transitions.begin(),
            m_asset.transitions.end(),
            [id](const GameLoopTransition& t) { return t.fromNodeId == id || t.toNodeId == id; }),
        m_asset.transitions.end());

    m_nodeViews.erase(
        std::remove_if(
            m_nodeViews.begin(),
            m_nodeViews.end(),
            [id](const NodeView& v) { return v.id == id; }),
        m_nodeViews.end());

    if (m_asset.startNodeId == id) m_asset.startNodeId = m_asset.nodes.empty() ? 0 : m_asset.nodes.front().id;

    ClearSelection();
    m_dirty = true;
}

void GameLoopEditorPanelInternal::DeleteTransition(int i)
{
    if (i < 0 || i >= (int)m_asset.transitions.size()) return;

    m_asset.transitions.erase(m_asset.transitions.begin() + i);
    ClearSelection();
    m_dirty = true;
}

void GameLoopEditorPanelInternal::ReverseTransition(int i)
{
    if (i < 0 || i >= (int)m_asset.transitions.size()) return;

    uint32_t old = m_asset.transitions[i].fromNodeId;
    m_asset.transitions[i].fromNodeId = m_asset.transitions[i].toNodeId;
    m_asset.transitions[i].toNodeId = old;
    m_dirty = true;
}

void GameLoopEditorPanelInternal::Validate()
{
    m_validateResult = ValidateGameLoopAsset(m_asset);
    m_validated = true;
}

void GameLoopEditorPanelInternal::Save()
{
    Validate();
    if (m_validateResult.HasError()) return;
    if (m_asset.SaveToFile(m_currentPath)) m_dirty = false;
}

void GameLoopEditorPanelInternal::Load(const std::filesystem::path& path)
{
    if (m_asset.LoadFromFile(path)) {
        m_currentPath = path;
        m_nodeViews.clear();
        ClearSelection();
        m_fitRequested = true;
        m_dirty = false;
        m_validated = false;
    }
}
