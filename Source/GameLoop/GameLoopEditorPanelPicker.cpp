// GameLoopEditorPanelPicker の GameLoop 関連実装をまとめます。
#include "GameLoopEditorPanelInternal.h"

void GameLoopEditorPanelInternal::DrawScenePickerPopup()
{
    std::string path;
    if (!m_scenePicker.Draw(PickerPopup, path)) return;

    if (m_pickerMode == PickerMode::CreateNode) {
        AddSceneNode(path, m_pickerGraphPos);
    }
    else if (m_pickerMode == PickerMode::ReplaceNode) {
        ReplaceNodeScene(m_pickerTargetNodeId, path);
    }
    else if (m_pickerMode == PickerMode::CreateNodeAndTransition) {
        AddSceneNode(path, m_pickerGraphPos);
        if (!m_asset.nodes.empty()) AddTransition(m_pickerFromNodeId, m_asset.nodes.back().id);
    }
    else if (m_pickerMode == PickerMode::SelectLoadingScene) {
        if (m_pickerTargetTransitionIndex >= 0 &&
            m_pickerTargetTransitionIndex < static_cast<int>(m_asset.transitions.size())) {
            m_asset.transitions[static_cast<size_t>(m_pickerTargetTransitionIndex)].loadingScenePath = path;
            m_dirty = true;
        }
    }

    m_pickerMode = PickerMode::None;
    m_pickerTargetNodeId = 0;
    m_pickerFromNodeId = 0;
    m_pickerTargetTransitionIndex = -1;
}

void GameLoopEditorPanelInternal::OpenPickerForCreate(const DirectX::XMFLOAT2& pos)
{
    m_pickerMode = PickerMode::CreateNode;
    m_pickerGraphPos = pos;
    m_scenePicker.Open(PickerPopup);
}

void GameLoopEditorPanelInternal::OpenPickerForReplace(uint32_t id)
{
    m_pickerMode = PickerMode::ReplaceNode;
    m_pickerTargetNodeId = id;
    m_scenePicker.Open(PickerPopup);
}

void GameLoopEditorPanelInternal::OpenPickerForConnection(uint32_t from, const DirectX::XMFLOAT2& pos)
{
    m_pickerMode = PickerMode::CreateNodeAndTransition;
    m_pickerFromNodeId = from;
    m_pickerGraphPos = pos;
    m_scenePicker.Open(PickerPopup);
}

void GameLoopEditorPanelInternal::OpenPickerForLoadingScene(int transitionIndex)
{
    m_pickerMode = PickerMode::SelectLoadingScene;
    m_pickerTargetTransitionIndex = transitionIndex;
    m_scenePicker.Open(PickerPopup);
}
