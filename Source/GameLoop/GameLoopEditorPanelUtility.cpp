#include "GameLoopEditorPanelInternal.h"

GameLoopNode* GameLoopEditorPanelInternal::FindNode(uint32_t id)
{
    return m_asset.FindNode(id);
}

const GameLoopNode* GameLoopEditorPanelInternal::FindNode(uint32_t id) const
{
    return m_asset.FindNode(id);
}

GameLoopEditorPanelInternal::NodeView* GameLoopEditorPanelInternal::FindView(uint32_t id)
{
    for (auto& v : m_nodeViews) if (v.id == id) return &v;
    return nullptr;
}

const GameLoopEditorPanelInternal::NodeView* GameLoopEditorPanelInternal::FindView(uint32_t id) const
{
    for (const auto& v : m_nodeViews) if (v.id == id) return &v;
    return nullptr;
}

DirectX::XMFLOAT2& GameLoopEditorPanelInternal::GetOrCreateNodePos(uint32_t id)
{
    if (NodeView* v = FindView(id)) return v->pos;

    float x = (float)m_nodeViews.size() * 320.0f;
    m_nodeViews.push_back({ id, { x, 0.0f } });
    return m_nodeViews.back().pos;
}

GameLoopTransition* GameLoopEditorPanelInternal::SelectedTransition()
{
    int i = SelectedTransitionIndex();
    return i >= 0 ? &m_asset.transitions[i] : nullptr;
}

int GameLoopEditorPanelInternal::SelectedTransitionIndex() const
{
    return m_selectedTransitionIndex >= 0 && m_selectedTransitionIndex < (int)m_asset.transitions.size() ? m_selectedTransitionIndex : -1;
}

std::string GameLoopEditorPanelInternal::ConditionLabel(const GameLoopCondition& c) const
{
    if (c.type == GameLoopConditionType::InputPressed) return ActionName(c.actionIndex);
    if (c.type == GameLoopConditionType::UIButtonClicked) return c.targetName.empty() ? "Button" : c.targetName;
    if (c.type == GameLoopConditionType::TimerElapsed) return "Timer " + std::to_string((int)c.seconds) + "s";
    if (c.type == GameLoopConditionType::AllActorsDead) return std::string("All ") + ActorName(c.actorType) + " Dead";
    if (c.type == GameLoopConditionType::ActorMovedDistance) return std::string(ActorName(c.actorType)) + " Moved";
    if (c.type == GameLoopConditionType::ActorDead) return std::string(ActorName(c.actorType)) + " Dead";
    return "Condition";
}

std::string GameLoopEditorPanelInternal::TransitionLabel(const GameLoopTransition& t) const
{
    if (t.conditions.empty()) return "No Condition";
    if (t.conditions.size() > 2) return "C:" + std::to_string(t.conditions.size());

    std::string r;
    for (size_t i = 0; i < t.conditions.size(); ++i) {
        if (i) r += t.requireAllConditions ? " AND " : " OR ";
        r += ConditionLabel(t.conditions[i]);
    }
    return r;
}

std::string GameLoopEditorPanelInternal::Ellipsis(const std::string& text, float width) const
{
    if (ImGui::CalcTextSize(text.c_str()).x <= width) return text;

    for (int keep = (int)text.size() - 1; keep > 6; --keep) {
        int l = keep / 2;
        int r = keep - l;
        std::string s = text.substr(0, l) + "..." + text.substr(text.size() - r, r);
        if (ImGui::CalcTextSize(s.c_str()).x <= width) return s;
    }

    return "...";
}

int GameLoopEditorPanelInternal::SceneStatus(const GameLoopNode& node) const
{
    if (node.scenePath.empty()) return 2;
    if (!GameLoopScenePicker::IsSceneAssetPath(node.scenePath)) return 2;

    std::error_code ec;
    if (!std::filesystem::exists(node.scenePath, ec)) return 1;

    return 0;
}
