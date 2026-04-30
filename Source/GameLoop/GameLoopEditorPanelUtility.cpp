#include "GameLoopEditorPanelInternal.h"

namespace
{
    struct ScancodeOption
    {
        uint32_t code;
        const char* label;
    };

    struct GamepadButtonOption
    {
        uint8_t code;
        const char* label;
    };

    const ScancodeOption* GetScancodeOptions(int& outCount)
    {
        static const ScancodeOption options[] = {
            { 0, "Unbound" },
            { 4, "A" }, { 5, "B" }, { 6, "C" }, { 7, "D" }, { 8, "E" }, { 9, "F" },
            { 10, "G" }, { 11, "H" }, { 12, "I" }, { 13, "J" }, { 14, "K" }, { 15, "L" },
            { 16, "M" }, { 17, "N" }, { 18, "O" }, { 19, "P" }, { 20, "Q" }, { 21, "R" },
            { 22, "S" }, { 23, "T" }, { 24, "U" }, { 25, "V" }, { 26, "W" }, { 27, "X" },
            { 28, "Y" }, { 29, "Z" },
            { 30, "1" }, { 31, "2" }, { 32, "3" }, { 33, "4" }, { 34, "5" },
            { 35, "6" }, { 36, "7" }, { 37, "8" }, { 38, "9" }, { 39, "0" },
            { 40, "Enter" }, { 41, "Escape" }, { 42, "Backspace" }, { 43, "Tab" }, { 44, "Space" },
            { 79, "Right" }, { 80, "Left" }, { 81, "Down" }, { 82, "Up" },
            { 224, "LCtrl" }, { 225, "LShift" }, { 226, "LAlt" },
            { 228, "RCtrl" }, { 229, "RShift" }, { 230, "RAlt" },
        };
        outCount = sizeof(options) / sizeof(options[0]);
        return options;
    }

    const GamepadButtonOption* GetGamepadButtonOptions(int& outCount)
    {
        static const GamepadButtonOption options[] = {
            { 0xFF, "Unbound" },
            { 0, "A / Cross" },
            { 1, "B / Circle" },
            { 2, "X / Square" },
            { 3, "Y / Triangle" },
            { 4, "Back" },
            { 5, "Guide" },
            { 6, "Start" },
            { 7, "Left Stick" },
            { 8, "Right Stick" },
            { 9, "Left Shoulder" },
            { 10, "Right Shoulder" },
            { 11, "DPad Up" },
            { 12, "DPad Down" },
            { 13, "DPad Left" },
            { 14, "DPad Right" },
        };
        outCount = sizeof(options) / sizeof(options[0]);
        return options;
    }

    const char* GetScancodeLabel(uint32_t scancode)
    {
        int count = 0;
        const ScancodeOption* options = GetScancodeOptions(count);
        for (int i = 0; i < count; ++i) {
            if (options[i].code == scancode) return options[i].label;
        }
        return "Custom";
    }

    const char* GetGamepadButtonLabel(uint8_t button)
    {
        int count = 0;
        const GamepadButtonOption* options = GetGamepadButtonOptions(count);
        for (int i = 0; i < count; ++i) {
            if (options[i].code == button) return options[i].label;
        }
        return "Custom";
    }
}

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

    if (GameLoopNode* node = FindNode(id)) {
        m_nodeViews.push_back({ id, node->graphPos });
        return m_nodeViews.back().pos;
    }

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

std::string GameLoopEditorPanelInternal::TransitionLabel(const GameLoopTransition& t) const
{
    const bool hasKeyboard = t.input.keyboardScancode != 0;
    const bool hasGamepad = t.input.gamepadButton != 0xFF;

    if (!hasKeyboard && !hasGamepad) return "Unbound";

    std::string label;
    if (hasKeyboard) {
        label += GetScancodeLabel(t.input.keyboardScancode);
    }
    if (hasGamepad) {
        if (!label.empty()) label += " / ";
        label += GetGamepadButtonLabel(t.input.gamepadButton);
    }
    return label;
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
