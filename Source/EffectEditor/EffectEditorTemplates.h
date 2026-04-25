#pragma once

class EffectEditorPanel;

// Template menu builder for EffectEditorPanel's toolbar.
// Caller wraps with ImGui::BeginPopup(..) / ImGui::EndPopup().
// Mutates panel.m_asset and side-effect flags via friend access.
class EffectEditorTemplates
{
public:
    static void DrawMenuContents(EffectEditorPanel& panel);
};
