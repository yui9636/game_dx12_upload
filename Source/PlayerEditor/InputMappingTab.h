#pragma once
#include "Input/InputActionMapAsset.h"

class Registry;
// Input Mapping tab drawn inside PlayerEditorPanel.
class InputMappingTab
{
public:
    enum class CaptureField {
        ActionKeyboard,
        ActionMouse,
        ActionGamepad,
        AxisPositiveKey,
        AxisNegativeKey,
        AxisGamepad
    };

    bool Draw(Registry* registry);

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
    void OpenBindingPopup(CaptureField field, int actionIndex, int axisIndex);
    void MarkChanged();

    InputActionMapAsset m_editingMap;
    bool m_dirty = false;
    bool m_changedThisFrame = false;

    // Pending input binding capture state.
    bool m_capturingKey = false;
    int  m_captureTargetAction = -1;
    int  m_captureTargetAxis = -1;
    int  m_captureSuppressFrames = 0;
    bool m_openBindingPopupRequested = false;
    char m_bindingSearch[64] = {};
    CaptureField m_captureField = CaptureField::ActionKeyboard;
};
