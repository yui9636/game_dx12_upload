#pragma once

class EffectEditorPanel;

// EffectEditorPanel の toolbar 用 template menu builder。
// 呼び出し側が ImGui::BeginPopup(..) / ImGui::EndPopup() で包む。
// friend access 経由で panel.m_asset と side-effect flag を変更する。
class EffectEditorTemplates
{
public:
    static void DrawMenuContents(EffectEditorPanel& panel);
};
