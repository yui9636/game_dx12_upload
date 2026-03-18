#include "InputActionComponent.h"
#include "Input/Input.h"
#include "Input/GamePad.h"
#include <cmath>
#include <imgui.h>

using namespace DirectX;

float InputActionComponent::Length2(const XMFLOAT2& v)
{
    return v.x * v.x + v.y * v.y;
}

XMFLOAT2 InputActionComponent::Normalize2D_Safe(const XMFLOAT2& v)
{
    float len2 = Length2(v);
    if (len2 <= 0.0000001f) return XMFLOAT2{ 0,0 };
    float inv = 1.0f / std::sqrt(len2);
    return XMFLOAT2{ v.x * inv, v.y * inv };
}

XMFLOAT2 InputActionComponent::ApplyDeadzoneAndNormalize(const XMFLOAT2& raw) const
{
    float r = moveDeadzone;
    if (r < 0.0f) r = 0.0f;
    if (r > 0.95f) r = 0.95f;

    float len2 = Length2(raw);
    if (len2 <= 0.0f) return XMFLOAT2{ 0,0 };
    float len = std::sqrt(len2);
    if (len < r) return XMFLOAT2{ 0,0 };

    // [r..1] → [0..1] へ線形リマップ
    float denom = (1.0f - r);
    float t = (denom > 0.0f) ? ((len - r) / denom) : 0.0f;
    if (t < 0.0f) t = 0.0f;
    if (t > 1.0f) t = 1.0f;

    XMFLOAT2 dir = Normalize2D_Safe(raw);
    return XMFLOAT2{ dir.x * t, dir.y * t };
}


void InputActionComponent::UpdateActionState(ActionState& st, bool held, bool pressed, bool released)
{
    st.pressed = pressed;
    st.released = released;
    st.held = held;

    if (pressed) {
        st.prevPressedFrame = st.lastPressedFrame;
        st.lastPressedFrame = frameCounter;
        st.framesSincePressed = 0;
    }
    else {
        int v = st.framesSincePressed + 1; if (v < 0) v = 0; st.framesSincePressed = v;
    }

    if (released) {
        st.framesSinceReleased = 0;
    }
    else {
        int v = st.framesSinceReleased + 1; if (v < 0) v = 0; st.framesSinceReleased = v;
    }

    if (st.cooldownFrames > 0) {
        st.cooldownFrames -= 1;
        if (st.cooldownFrames < 0) st.cooldownFrames = 0;
    }
}


void InputActionComponent::PushBufferedIfAvailable(ActionType type, const ActionState& st)
{
    int i = (int)type;
    if (!st.pressed) return;
    if (st.cooldownFrames > 0) return;
    if (!actionConfig[i].enabled) return;

    // 固定長リング：満杯なら最古を捨てる
    if (bufferCount >= BufferCap) {
        bufferHead += 1; if (bufferHead >= BufferCap) bufferHead = 0;
        bufferCount -= 1; if (bufferCount < 0) bufferCount = 0;
    }
    int tail = bufferHead + bufferCount; while (tail >= BufferCap) tail -= BufferCap;
    buffer[tail] = BufferedAction{ type, frameCounter };
    bufferCount += 1;
}

void InputActionComponent::Start()
{
    output = Output{};
    frameAccumulator = 0.0f;
    frameCounter = 0;
    bufferHead = 0;
    bufferCount = 0;
    moveDeadzone = 0.15f;

    // アクション設定初期化
    for (int i = 0; i < (int)ActionType::Count; ++i) {
        actionConfig[i] = ActionConfig{};
        bindTable[i] = ActionBindSet{};
    }
    // 例：Dodge は基準CDを少し与える
    actionConfig[(int)ActionType::Dodge].cooldownDefault = 20;

    // === 既定の GamePad 割当（XInput想定） ===
    // BTN_* は GamePad クラスの定義を利用するのじゃ
    bindTable[(int)ActionType::Dodge].pad.heldMask = GamePad::BTN_RIGHT_SHOULDER;
    bindTable[(int)ActionType::Dodge].pad.downMask = GamePad::BTN_RIGHT_SHOULDER;
    bindTable[(int)ActionType::Dodge].pad.upMask = GamePad::BTN_RIGHT_SHOULDER;

    bindTable[(int)ActionType::Parry].pad.heldMask = GamePad::BTN_LEFT_SHOULDER;
    bindTable[(int)ActionType::Parry].pad.downMask = GamePad::BTN_LEFT_SHOULDER;
    bindTable[(int)ActionType::Parry].pad.upMask = GamePad::BTN_LEFT_SHOULDER;

    bindTable[(int)ActionType::AttackLight].pad.heldMask = (GamePad::BTN_A | GamePad::BTN_X);
    bindTable[(int)ActionType::AttackLight].pad.downMask = (GamePad::BTN_A | GamePad::BTN_X);
    bindTable[(int)ActionType::AttackLight].pad.upMask = (GamePad::BTN_A | GamePad::BTN_X);

    bindTable[(int)ActionType::AttackHeavy].pad.heldMask = (GamePad::BTN_B | GamePad::BTN_Y);
    bindTable[(int)ActionType::AttackHeavy].pad.downMask = (GamePad::BTN_B | GamePad::BTN_Y);
    bindTable[(int)ActionType::AttackHeavy].pad.upMask = (GamePad::BTN_B | GamePad::BTN_Y);

    bindTable[(int)ActionType::LockOn].pad.heldMask = GamePad::BTN_LEFT_THUMB;
    bindTable[(int)ActionType::LockOn].pad.downMask = GamePad::BTN_LEFT_THUMB;
    bindTable[(int)ActionType::LockOn].pad.upMask = GamePad::BTN_LEFT_THUMB;

    bindTable[(int)ActionType::Jump].pad.heldMask = GamePad::BTN_RIGHT_THUMB;
    bindTable[(int)ActionType::Jump].pad.downMask = GamePad::BTN_RIGHT_THUMB;
    bindTable[(int)ActionType::Jump].pad.upMask = GamePad::BTN_RIGHT_THUMB;

    // === 既定の Keyboard 割当（WASD / J,K,L,U / Space） ===
    int atkL_down[8] = { 'J', 0 };
    int atkH_down[8] = { 'K', 0 };
    int dodge_down[8] = { 'L', 0 };
    int parry_down[8] = { 'U', 0 };
    int jump_down[8] = { VK_SPACE, 0 };
    SetKeyboardBinding(ActionType::AttackLight, nullptr, atkL_down, nullptr);
    SetKeyboardBinding(ActionType::AttackHeavy, nullptr, atkH_down, nullptr);
    SetKeyboardBinding(ActionType::Dodge, nullptr, dodge_down, nullptr);
    SetKeyboardBinding(ActionType::Parry, nullptr, parry_down, nullptr);
    SetKeyboardBinding(ActionType::Jump, nullptr, jump_down, nullptr);

    // GUI関連
    guiOpenRebind = false;
    listenDev = ListenDevice::None;
    listenEdge = ListenEdge::Down;
    listenActionIndex = -1;
    listenSlotIndex = -1;
    lastKeyboardVk = 0;
    lastGpDownMask = 0;
}

/**
 * @brief 固定FPSを設定（フレーム換算の基準）
 */
void InputActionComponent::SetFixedFps(float fps)
{
    if (fps < 1.0f) fps = 1.0f;
    fixedFps = fps;
}

/**
 * @brief デッドゾーン半径を設定（0.0?0.95）
 */
void InputActionComponent::SetMoveDeadzone(float r)
{
    if (r < 0.0f) r = 0.0f;
    if (r > 0.95f) r = 0.95f;
    moveDeadzone = r;
}


void InputActionComponent::CopyKeys8(int dst[8], const int* src)
{
    int i = 0;
    if (src) { while (i < 7 && src[i] != 0) { dst[i] = src[i]; ++i; } }
    while (i < 8) { dst[i] = 0; ++i; }
}

/**
 * @brief Keyboard割当（held/down/up）をまとめて設定するのじゃ
 */
void InputActionComponent::SetKeyboardBinding(ActionType a, const int* heldKeys, const int* downKeys, const int* upKeys)
{
    int idx = (int)a;
    CopyKeys8(bindTable[idx].kb.heldKeys, heldKeys);
    CopyKeys8(bindTable[idx].kb.downKeys, downKeys);
    CopyKeys8(bindTable[idx].kb.upKeys, upKeys);
}

/**
 * @brief GamePad/Keyboard の生入力を読み、論理フラグへマッピングするのじゃ
 */
void InputActionComponent::SampleDeviceAndMap()
{
    // クリア
    for (int i = 0; i < (int)ActionType::Count; ++i) { rawHeld[i] = rawDown[i] = rawUp[i] = false; }
    rawMove = XMFLOAT2{ 0,0 };

    // --- GamePad 生入力 ---
    GamePad& gp = Input::Instance().GetGamePad();
    const uint32_t btn = (uint32_t)gp.GetButton();
    const uint32_t btnDown = (uint32_t)gp.GetButtonDown();
    const uint32_t btnUp = (uint32_t)gp.GetButtonUp();

    // スティック
    rawMove.x = gp.GetAxisLX();
    rawMove.y = gp.GetAxisLY();

    // 可視化用
    lastGpDownMask = btnDown;

    // --- Keyboard 生入力 ---
    for (int vk = 0; vk <= 0xFF; ++vk) {
        SHORT s = GetAsyncKeyState(vk);
        keyCurr[vk] = ((s & 0x8000) != 0);
    }

    // 移動（WASD をスティック合成）
    if (keyCurr['W']) rawMove.y += 1.0f;
    if (keyCurr['S']) rawMove.y -= 1.0f;
    if (keyCurr['A']) rawMove.x -= 1.0f;
    if (keyCurr['D']) rawMove.x += 1.0f;

    // 斜めで √2 超過を正規化
    float len2 = Length2(rawMove);
    if (len2 > 1.0f) {
        float inv = 1.0f / std::sqrt(len2);
        rawMove.x *= inv; rawMove.y *= inv;
    }

    // 直近の KeyDown（GUI用）
    lastKeyboardVk = 0;
    for (int vk = 0x08; vk <= 0xFE; ++vk) {
        if (!keyPrev[vk] && keyCurr[vk]) { lastKeyboardVk = vk; break; }
    }

    // --- アクション毎にバインド評価（Pad/KB の OR） ---
    for (int i = 0; i < (int)ActionType::Count; ++i)
    {
        const ActionBindSet& B = bindTable[i];

        if (!actionConfig[i].enabled || !B.enabled) {
            rawHeld[i] = rawDown[i] = rawUp[i] = false;
            continue;
        }
        // GamePad
        bool heldPad = (B.pad.heldMask != 0) && ((btn & B.pad.heldMask) != 0);
        bool downPad = (B.pad.downMask != 0) && ((btnDown & B.pad.downMask) != 0);
        bool upPad = (B.pad.upMask != 0) && ((btnUp & B.pad.upMask) != 0);

        // Keyboard（0終端配列を総当たり）
        auto anyKeyVK = [&](const int keys[8], int edge)->bool {
            int k = 0;
            while (k < 8 && keys[k] != 0) {
                int vk = keys[k];
                bool on = (edge == 0) ? keyCurr[vk]
                    : (edge == 1) ? (!keyPrev[vk] && keyCurr[vk])
                    : (keyPrev[vk] && !keyCurr[vk]);
                if (on) return true;
                ++k;
            }
            return false;
            };
        bool heldKb = anyKeyVK(B.kb.heldKeys, 0);
        bool downKb = anyKeyVK(B.kb.downKeys, 1);
        bool upKb = anyKeyVK(B.kb.upKeys, 2);

        // 両デバイスの OR
        rawHeld[i] = (heldPad || heldKb);
        rawDown[i] = (downPad || downKb);
        rawUp[i] = (upPad || upKb);
    }

    // --- リッスン（簡易リバインド） ---
    if (listenDev != ListenDevice::None && listenActionIndex >= 0)
    {
        if (listenDev == ListenDevice::GamePad) {
            if (btnDown != 0) {
                if (listenEdge == ListenEdge::Down) bindTable[listenActionIndex].pad.downMask = btnDown;
                else if (listenEdge == ListenEdge::Up) bindTable[listenActionIndex].pad.upMask = btnDown;
                else bindTable[listenActionIndex].pad.heldMask = btnDown;
                listenDev = ListenDevice::None; listenActionIndex = -1; listenSlotIndex = -1;
            }
        }
        else {
            int slot = (listenSlotIndex < 0) ? 0 : listenSlotIndex;
            if (slot < 0) slot = 0; if (slot > 6) slot = 6;
            if (lastKeyboardVk != 0) {
                int* dst = (listenEdge == ListenEdge::Down) ? bindTable[listenActionIndex].kb.downKeys
                    : (listenEdge == ListenEdge::Up) ? bindTable[listenActionIndex].kb.upKeys
                    : bindTable[listenActionIndex].kb.heldKeys;
                dst[slot] = lastKeyboardVk;
                if (slot + 1 < 8) dst[slot + 1] = 0;
                listenDev = ListenDevice::None; listenActionIndex = -1; listenSlotIndex = -1;
            }
        }
    }

    // --- prev を更新 ---
    for (int vk = 0; vk <= 0xFF; ++vk) keyPrev[vk] = keyCurr[vk];
}

//==============================================================
// メイン更新
//==============================================================

/**
 * @brief 1フレーム更新：時間を進め、デバイス→論理化→先行入力積みを行うのじゃ
 */
void InputActionComponent::Update(float dt)
{
    // 入力時間（フレーム）を進める
    frameAccumulator += dt * fixedFps;
    if (frameAccumulator >= 1.0f) {
        int adv = static_cast<int>(frameAccumulator);
        if (adv < 1) adv = 1;
        frameCounter += adv;
        frameAccumulator -= static_cast<float>(adv);
    }

    // 1) デバイス→論理の正規化
    SampleDeviceAndMap();

    // 2) 移動ベクトル（デッドゾーン適用→正規化）
    output.move = ApplyDeadzoneAndNormalize(rawMove);

    // 3) 各アクション状態を更新＆必要なら先行入力へ積む
    for (int i = 0; i < (int)ActionType::Count; ++i)
    {
        ActionState& st = output.actions[i];
        UpdateActionState(st, rawHeld[i], rawDown[i], rawUp[i]);
        PushBufferedIfAvailable(static_cast<ActionType>(i), st);
    }
}

/**
 * @brief 先行入力を消費（受付窓内なら true）
 */
bool InputActionComponent::ConsumeBuffered(ActionType type, int maxAcceptFrames)
{
    int window = maxAcceptFrames;
    if (window < 0) {
        window = actionConfig[(int)type].acceptFrames;
        if (window < 0) window = 0;
    }
    if (bufferCount <= 0) return false;

    // 先頭から探す（成立順で処理）
    for (int k = 0; k < bufferCount; ++k)
    {
        int idx = bufferHead + k; while (idx >= BufferCap) idx -= BufferCap;

        if (buffer[idx].type == type)
        {
            int age = frameCounter - buffer[idx].frameStamp;
            if (age < 0) age = 0;
            bool ok = (age <= window);

            // 要素を抜いて詰める
            for (int m = k; m < bufferCount - 1; ++m)
            {
                int a = bufferHead + m;       while (a >= BufferCap) a -= BufferCap;
                int b = bufferHead + (m + 1); while (b >= BufferCap) b -= BufferCap;
                buffer[a] = buffer[b];
            }
            bufferCount -= 1;
            if (bufferCount < 0) bufferCount = 0;
            return ok;
        }
    }
    return false;
}

/**
 * @brief バッファを覗き見る（消費しない）。受理可能なら true
 */
bool InputActionComponent::PeekBuffered(ActionType type, int maxAcceptFrames) const
{
    int window = maxAcceptFrames;
    if (window < 0) {
        window = actionConfig[(int)type].acceptFrames;
        if (window < 0) window = 0;
    }
    if (bufferCount <= 0) return false;

    for (int k = 0; k < bufferCount; ++k)
    {
        int idx = bufferHead + k; while (idx >= BufferCap) idx -= BufferCap;
        if (buffer[idx].type == type)
        {
            int age = frameCounter - buffer[idx].frameStamp;
            if (age < 0) age = 0;
            return (age <= window);
        }
    }
    return false;
}

/**
 * @brief “要求スナップショット”を構築して返す（必要ならバッファ消費）
 * @details
 *  - まず先行入力（Consume or Peek）を見て要求フラグを立てる
 *  - 先行入力が無ければ「今フレーム pressed」でも要求を立てる
 *  - 代表例としてダブルタップや長押しの判定も入れておくのじゃ
 */
void InputActionComponent::BuildActionRequest(ActionRequest& out, bool consumeBuffered, int acceptFramesOverride)
{
    out = ActionRequest{};
    out.frameStamp = frameCounter;
    out.move = output.move;

    auto want = [&](ActionType t)->bool {
        if (consumeBuffered)  return ConsumeBuffered(t, acceptFramesOverride);
        else                  return PeekBuffered(t, acceptFramesOverride);
        };

    // 先行入力 or 今フレーム押下で要求ON
    out.attackLight = want(ActionType::AttackLight) || output.actions[(int)ActionType::AttackLight].pressed;
    out.attackHeavy = want(ActionType::AttackHeavy) || output.actions[(int)ActionType::AttackHeavy].pressed;
    out.dodge = want(ActionType::Dodge) || output.actions[(int)ActionType::Dodge].pressed;
    out.parry = want(ActionType::Parry) || output.actions[(int)ActionType::Parry].pressed;
    out.lockOn = want(ActionType::LockOn) || output.actions[(int)ActionType::LockOn].pressed;
    out.jump = want(ActionType::Jump) || output.actions[(int)ActionType::Jump].pressed;

    // 参考：特殊判定
    out.wasDoubleTapDodge = IsDoubleTap(ActionType::Dodge);
    out.wasLongPressHeavy = IsLongPress(ActionType::AttackHeavy);
}

/**
 * @brief アクションにクールダウンを与えるのじゃ
 */
void InputActionComponent::SetCooldown(ActionType type, int frames)
{
    if (frames < 0) frames = 0;
    output.actions[(int)type].cooldownFrames = frames;
}

/**
 * @brief GamePadボタンの割当を設定するのじゃ
 */
void InputActionComponent::SetGamePadBinding(ActionType a, uint32_t held, uint32_t down, uint32_t up)
{
    int i = (int)a;
    bindTable[i].pad.heldMask = held;
    bindTable[i].pad.downMask = down;
    bindTable[i].pad.upMask = up;
}

//==============================================================
// GUI（必要なければ空のままでOK）
//==============================================================

/**
 * @brief 簡易状態確認GUI（Pressed/Held/Releasedやバッファ）
 */
void InputActionComponent::DebugGUI()
{
    if (!ImGui::Begin("Input")) { ImGui::End(); return; }
    ImGui::Text("Frame: %d", frameCounter);
    ImGui::Text("Move: (%.2f, %.2f)", output.move.x, output.move.y);

    const char* names[(int)ActionType::Count] = { "Dodge","Parry","Light","Heavy","LockOn","Jump" };
    for (int i = 0; i < (int)ActionType::Count; ++i) {
        const ActionState& s = output.actions[i];
        ImGui::Text("%-6s  P:%d H:%d R:%d  CD:%d  sinceP:%d",
            names[i], s.pressed ? 1 : 0, s.held ? 1 : 0, s.released ? 1 : 0, s.cooldownFrames, s.framesSincePressed);
    }
    ImGui::Separator();
    ImGui::Text("Buffered: %d", bufferCount);
    for (int k = 0; k < bufferCount; ++k) {
        int idx = bufferHead + k; while (idx >= BufferCap) idx -= BufferCap;
        int ti = (int)buffer[idx].type; if (ti < 0) ti = 0; if (ti >= (int)ActionType::Count) ti = (int)ActionType::Count - 1;
        ImGui::Text("  [%02d] %s age=%d", k, names[ti], frameCounter - buffer[idx].frameStamp);
    }
    ImGui::End();
}

/**
 * @brief リバインドGUI（簡易）
 */
void InputActionComponent::DebugGUI_Rebind()
{
    if (!ImGui::Begin("InputAction Rebind")) { ImGui::End(); return; }
    ImGui::Text("Last KB Down VK: %d", lastKeyboardVk);
    ImGui::Text("Last GP Down  : 0x%08X", (unsigned)lastGpDownMask);
    ImGui::End();
}

/**
 * @brief コンパクト版パネル
 */
void InputActionComponent::DrawCompactPanel()
{
    ImGui::Text("Move(%.2f,%.2f)", output.move.x, output.move.y);
}

/**
 * @brief HUDオーバーレイ
 */
void InputActionComponent::DrawHudOverlay(bool* on, int /*corner*/)
{
    if (!on || !*on) return;
    ImGui::Begin("Input HUD", on, ImGuiWindowFlags_AlwaysAutoResize);
    DrawCompactPanel();
    ImGui::End();
}

/**
 * @brief ポップアップ開始
 */
void InputActionComponent::OpenRebindPopup()
{
    guiOpenRebind = true;
}

/**
 * @brief ポップアップ本体
 */
void InputActionComponent::DrawRebindPopup()
{
    if (!guiOpenRebind) return;
    ImGui::OpenPopup("Rebind (WIP)");
    if (ImGui::BeginPopupModal("Rebind (WIP)", &guiOpenRebind, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::TextUnformatted("Press a GamePad button or a Keyboard key...");
        if (ImGui::Button("Close")) { guiOpenRebind = false; }
        ImGui::EndPopup();
    }
}
