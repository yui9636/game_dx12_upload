#include "GameLoopEditorPanelInternal.h"

namespace
{
    const char* DefaultNodeName(GameLoopNodeType type)
    {
        switch (type) {
        case GameLoopNodeType::Scene: return "Scene";
        case GameLoopNodeType::State: return "State";
        case GameLoopNodeType::Event: return "Event";
        case GameLoopNodeType::Action:return "Action";
        case GameLoopNodeType::Battle:return "Battle";
        }
        return "State";
    }

    std::string DefaultBattleId(const GameLoopNode& node)
    {
        return node.name.empty() ? std::string{ "default" } : node.name;
    }

    void AddDefaultActionsForNode(const GameLoopNode& toNode, std::vector<GameFlowAction>& actions)
    {
        if (toNode.type == GameLoopNodeType::Scene) {
            return;
        }

        if (toNode.type == GameLoopNodeType::Battle) {
            GameFlowAction battle;
            battle.type = GameFlowActionType::StartBattleFlow;
            battle.target = DefaultBattleId(toNode);
            actions.push_back(battle);
        }

        GameFlowAction setNode;
        setNode.type = GameFlowActionType::SetCurrentNode;
        actions.push_back(setNode);
    }
}

void GameLoopEditorPanelInternal::AddSceneNode(const std::string& path, const DirectX::XMFLOAT2& pos)
{
    std::string norm = GameLoopScenePicker::NormalizeScenePath(path);
    if (norm.empty()) return;

    GameLoopNode n;
    n.id = m_asset.AllocateNodeId();
    n.name = GameLoopScenePicker::BuildNodeNameFromScenePath(norm);
    n.scenePath = norm;
    n.type = GameLoopNodeType::Scene;
    n.graphPos = pos;

    m_asset.nodes.push_back(n);
    m_nodeViews.push_back({ n.id, pos });

    if (m_asset.startNodeId == 0) m_asset.startNodeId = n.id;

    SelectNode(n.id);
    m_dirty = true;
}

void GameLoopEditorPanelInternal::AddFlowNode(GameLoopNodeType type, const std::string& name, const DirectX::XMFLOAT2& pos)
{
    if (type == GameLoopNodeType::Scene) {
        OpenPickerForCreate(pos);
        return;
    }

    GameLoopNode n;
    n.id = m_asset.AllocateNodeId();
    n.name = name.empty() ? DefaultNodeName(type) : name;
    n.type = type;
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

    const std::string oldPath = n->scenePath;
    n->type = GameLoopNodeType::Scene;
    n->scenePath = norm;
    n->name = GameLoopScenePicker::BuildNodeNameFromScenePath(norm);
    for (GameLoopTransition& transition : m_asset.transitions) {
        if (transition.toNodeId != id) continue;
        for (GameFlowAction& action : transition.actions) {
            if (action.type == GameFlowActionType::LoadScene &&
                (action.target.empty() || action.target == oldPath)) {
                action.target = norm;
            }
        }
    }
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
    t.conditionMode = GameFlowConditionMode::All;

    GameFlowCondition condition;
    condition.type = GameFlowConditionType::UIButtonClick;
    condition.value.clear();
    t.conditions.push_back(condition);

    if (const GameLoopNode* toNode = FindNode(to)) {
        AddDefaultActionsForNode(*toNode, t.actions);
    }

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
    if (const GameLoopNode* toNode = FindNode(m_asset.transitions[i].toNodeId)) {
        for (GameFlowAction& action : m_asset.transitions[i].actions) {
            if (action.type == GameFlowActionType::LoadScene && toNode->type == GameLoopNodeType::Scene) {
                action.target = toNode->scenePath;
            }
            else if (action.type == GameFlowActionType::StartBattleFlow && toNode->type == GameLoopNodeType::Battle) {
                action.target = DefaultBattleId(*toNode);
            }
        }
    }
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
    if (m_asset.SaveToFile(m_currentPath)) {
        m_dirty = false;
        m_fileDialogMode = FileDialogMode::None;
        m_statusMessage = "Saved: " + m_currentPath.string();
        if (m_validateResult.HasError()) {
            m_statusMessage += " (with validation errors)";
        }
    }
    else {
        m_statusMessage = "Save failed: " + m_currentPath.string();
    }
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
        m_fileDialogMode = FileDialogMode::None;
        m_statusMessage = "Loaded: " + m_currentPath.string();
    }
    else {
        m_statusMessage = "Load failed: " + path.string();
    }
}
