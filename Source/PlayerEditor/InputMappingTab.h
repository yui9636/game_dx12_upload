#pragma once
#include "Input/InputActionMapAsset.h"
#include <string>

class Registry;

// ============================================================================
// Input Mapping Editor Tab - draws inside PlayerEditorPanel
// ============================================================================

class InputMappingTab
{
public:
    void Draw(Registry* registry);

    bool OpenActionMap(const std::string& path);
    bool SaveActionMap();
    bool SaveActionMapAs(const std::string& path);
    bool ReloadActionMap();
    void SetActionMapPath(const std::string& path);
    const std::string& GetActionMapPath() const { return m_actionMapPath; }
    const InputActionMapAsset& GetEditingMap() const { return m_editingMap; }
    bool IsDirty() const { return m_dirty; }

private:
    void DrawActionTable();
    void DrawAxisTable();
    void DrawSettings();
    void DrawLiveTest(Registry* registry);
    void DrawKeyBindPopup();

    std::string m_actionMapPath;
    InputActionMapAsset m_editingMap;
    bool m_dirty = false;

    // Key bind capture state
    bool m_capturingKey = false;
    int  m_captureTargetAction = -1;
    enum class CaptureField { Keyboard, Mouse, Gamepad };
    CaptureField m_captureField = CaptureField::Keyboard;
};
