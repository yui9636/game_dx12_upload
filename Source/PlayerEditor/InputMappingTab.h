#pragma once
#include "Input/InputActionMapAsset.h"

class Registry;
// PlayerEditorPanel 内に描画される Input Mapping タブ。
class InputMappingTab
{
public:
    void Draw(Registry* registry);

    void SetEditingMap(const InputActionMapAsset& map);
    InputActionMapAsset& GetEditingMapMutable() { return m_editingMap; }
    void ClearEditingMap();
    const InputActionMapAsset& GetEditingMap() const { return m_editingMap; }
    bool IsDirty() const { return m_dirty; }
    void MarkDirty() { m_dirty = true; }

private:
    void DrawActionTable();
    void DrawAxisTable();
    void DrawSettings();
    void DrawLiveTest(Registry* registry);
    void DrawKeyBindPopup();

    InputActionMapAsset m_editingMap;
    bool m_dirty = false;

    // キー割り当て待ちの入力状態。
    bool m_capturingKey = false;
    int  m_captureTargetAction = -1;
    enum class CaptureField { Keyboard, Mouse, Gamepad };
    CaptureField m_captureField = CaptureField::Keyboard;
};
