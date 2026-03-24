#pragma once
#include "Component/Component.h"
#include <DirectXMath.h>
#include <cstdint>
#include <string>
#include <windows.h>
#include <imgui.h>

/**
 * @brief GamePad/Keyboard ・ｽﾌ撰ｿｽ・ｽ・ｽ・ｽﾍゑｿｽu・ｽ_・ｽ・ｽ・ｽA・ｽN・ｽV・ｽ・ｽ・ｽ・ｽ・ｽv・ｽﾉ撰ｿｽ・ｽK・ｽ・ｽ・ｽ・ｽ・ｽA
 *        ・ｽ・ｽ・ｽt・ｽ・ｽ・ｽ[・ｽ・ｽ・ｽﾌ“・ｽv・ｽ・ｽ・ｽX・ｽi・ｽb・ｽv・ｽV・ｽ・ｽ・ｽb・ｽg・ｽh・ｽｶ撰ｿｽ・ｽﾅゑｿｽ・ｽ・ｽ・ｽ・ｽﾍ層・ｽﾈのゑｿｽ・ｽ・ｽ
 * @details
 *  - ・ｽf・ｽo・ｽC・ｽX・ｽﾋ托ｿｽ・ｽﾍゑｿｽ・ｽ・ｽ・ｽﾉ閉ゑｿｽ・ｽ・ｽ・ｽﾟ、・ｽ・ｽﾊは「・ｽv・ｽ・ｽ(ActionRequest)・ｽv・ｽ・ｽ・ｽ・ｽ驍ｾ・ｽ・ｽ・ｽﾅ良ゑｿｽ・ｽﾝ計・ｽﾉゑｿｽ・ｽ・ｽﾌゑｿｽ・ｽ・ｽ
 *  - ・ｽ・ｽs・ｽ・ｽ・ｽﾍバ・ｽb・ｽt・ｽ@・ｽA・ｽ・ｽ・ｽ・ｽ・ｽ・ｽ/・ｽ_・ｽu・ｽ・ｽ・ｽ^・ｽb・ｽv・ｽA・ｽN・ｽ[・ｽ・ｽ・ｽ_・ｽE・ｽ・ｽ・ｽ・ｽﾇ暦ｿｽ
 *  - GUI・ｽﾅ・ｿｽ・ｽo・ｽC・ｽ・ｽ・ｽh・ｽ・ｽﾂ能・ｽi・ｽ・ｽ・ｽ・ｽ・ｽﾍ簡易／・ｽK・ｽv・ｽﾈゑｿｽ・ｽ・ｽﾎ呼ばゑｿｽﾊ）
 *  - ・ｽ・ｽ・ｽﾓ：std::min/std::max ・ｽﾍ使・ｽ・ｽﾊ・・ｽ・ｽ・ｽﾔ変撰ｿｽ・ｽ・ｽ・ｽ・ｽ dt ・ｽﾉ難ｿｽ・ｽ・ｽ
 */
class InputActionComponent final : public Component
{
public:
    const char* GetName() const override { return "InputAction"; }

    enum class ActionType : uint32_t
    {
        Dodge = 0,
        Parry,
        AttackLight,
        AttackHeavy,
        LockOn,
        Jump,
        Count
    };

    struct ActionState
    {
        bool pressed = false;
        bool released = false;
        bool held = false;

        int framesSincePressed = 1 << 28;
        int framesSinceReleased = 1 << 28;
        int cooldownFrames = 0;

        int lastPressedFrame = -999999;
        int prevPressedFrame = -999999;
    };

    struct ActionConfig
    {
        int acceptFrames = 7;
        int cooldownDefault = 0;
        int longPressFrames = 15;
        int doubleTapGap = 10;
        bool enabled = true;
    };

    struct Output
    {
        DirectX::XMFLOAT2 move{ 0,0 };
        ActionState actions[(int)ActionType::Count];
    };

    /**
     * @brief ・ｽ・ｽﾊ・ｿｽ・ｽW・ｽb・ｽN・ｽ・ｽ・ｽﾇみ趣ｿｽ・ｽu・ｽ・ｽ・ｽﾌフ・ｽ・ｽ・ｽ[・ｽ・ｽ・ｽﾅ趣ｿｽ・ｽs・ｽ・ｽ・ｽ・ｽ・ｽ・ｽ・ｽv・ｽ・ｽ・ｽv・ｽﾌまとめなのゑｿｽ・ｽ・ｽ
     * @details
     *  - ・ｽ・ｽ{・ｽﾍ撰ｿｽs・ｽ・ｽ・ｽﾍバ・ｽb・ｽt・ｽ@・ｽ・ｽQ・ｽﾆゑｿｽ・ｽ・ｽ True ・ｽﾉなゑｿｽi・ｽ・ｽ・ｽo・ｽb・ｽt・ｽ@・ｽ・ｽ・ｽﾍ搾ｿｽ・ｽt・ｽ・ｽ・ｽ[・ｽ・ｽ pressed ・ｽ・ｽ・ｽ・ｽ・ｽj
     *  - consumeBuffered=true ・ｽﾌとゑｿｽ・ｽﾍ該・ｽ・ｽ・ｽA・ｽN・ｽV・ｽ・ｽ・ｽ・ｽ・ｽﾌバ・ｽb・ｽt・ｽ@・ｽ・ｽ・ｽ・ｽ・ｷ・ｽ・ｽ
     */
    struct ActionRequest
    {
        DirectX::XMFLOAT2 move{ 0,0 };
        bool attackLight = false;
        bool attackHeavy = false;
        bool dodge = false;
        bool parry = false;
        bool lockOn = false;
        bool jump = false;

        int frameStamp = 0;
        bool wasDoubleTapDodge = false;
        bool wasLongPressHeavy = false;
    };

    struct GamePadBinding { uint32_t heldMask = 0, downMask = 0, upMask = 0; };
    struct KeyboardBinding { int heldKeys[8] = { 0 }; int downKeys[8] = { 0 }; int upKeys[8] = { 0 }; };
    struct ActionBindSet { GamePadBinding pad; KeyboardBinding kb; bool enabled = true; };

public:
    void Start() override;

    void Update(float dt) override;

    void Render() override {}

    const Output& GetOutput() const { return output; }

    bool ConsumeBuffered(ActionType type, int maxAcceptFrames = -1);

    bool PeekBuffered(ActionType type, int maxAcceptFrames = -1) const;

    void BuildActionRequest(ActionRequest& out, bool consumeBuffered = true, int acceptFramesOverride = -1);

    void SetCooldown(ActionType type, int frames);

    int GetFrameCounter() const { return frameCounter; }

    void SetFixedFps(float fps);

    void SetMoveDeadzone(float r);

    void SetGamePadBinding(ActionType a, uint32_t held, uint32_t down, uint32_t up);

    void SetKeyboardBinding(ActionType a, const int* heldKeys, const int* downKeys, const int* upKeys);

    void SetActionConfig(ActionType a, const ActionConfig& cfg);

    bool IsLongPress(ActionType a) const;

    bool IsDoubleTap(ActionType a) const;

    void DebugGUI();

    void DebugGUI_Rebind();

    void DrawCompactPanel();

    void DrawHudOverlay(bool* on, int corner = 1);

    void OpenRebindPopup();

    void DrawRebindPopup();

private:
    static float Length2(const DirectX::XMFLOAT2& v);
    static DirectX::XMFLOAT2 Normalize2D_Safe(const DirectX::XMFLOAT2& v);
    DirectX::XMFLOAT2 ApplyDeadzoneAndNormalize(const DirectX::XMFLOAT2& raw) const;

    void UpdateActionState(ActionState& st, bool held, bool pressed, bool released);

    void PushBufferedIfAvailable(ActionType type, const ActionState& st);

    static void CopyKeys8(int dst[8], const int* src);

    void SampleDeviceAndMap();

private:
    Output output{};

    DirectX::XMFLOAT2 rawMove{ 0,0 };
    bool rawHeld[(int)ActionType::Count]{};
    bool rawDown[(int)ActionType::Count]{};
    bool rawUp[(int)ActionType::Count]{};

    ActionConfig actionConfig[(int)ActionType::Count]{};
    ActionBindSet bindTable[(int)ActionType::Count]{};

    float frameAccumulator = 0.0f;
    int   frameCounter = 0;
    float fixedFps = 60.0f;
    float moveDeadzone = 0.15f;

    struct BufferedAction { ActionType type; int frameStamp; };
    static constexpr int BufferCap = 32;
    BufferedAction buffer[BufferCap]{};
    int bufferHead = 0;
    int bufferCount = 0;

    bool keyPrev[256]{};
    bool keyCurr[256]{};
    int  lastKeyboardVk = 0;

    enum class ListenDevice { None, GamePad, Keyboard };
    enum class ListenEdge { Held, Down, Up };
    bool guiOpenRebind = false;
    ListenDevice listenDev = ListenDevice::None;
    ListenEdge   listenEdge = ListenEdge::Down;
    int  listenActionIndex = -1;
    int  listenSlotIndex = -1;
    uint32_t lastGpDownMask = 0;
};

inline void InputActionComponent::SetActionConfig(ActionType a, const ActionConfig& cfg) { actionConfig[(int)a] = cfg; }
inline bool  InputActionComponent::IsLongPress(ActionType a) const { const ActionState& st = output.actions[(int)a]; return (st.held && (st.framesSincePressed >= actionConfig[(int)a].longPressFrames)); }
inline bool  InputActionComponent::IsDoubleTap(ActionType a) const { const ActionState& st = output.actions[(int)a]; int gap = st.lastPressedFrame - st.prevPressedFrame; return (gap >= 0 && gap <= actionConfig[(int)a].doubleTapGap); }
