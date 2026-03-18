#pragma once
#include "Component/Component.h"
#include <DirectXMath.h>
#include <cstdint>
#include <string>
#include <windows.h>
#include <imgui.h>

/**
 * @brief GamePad/Keyboard の生入力を「論理アクション」に正規化し、
 *        毎フレームの“要求スナップショット”を生成できる入力層なのじゃ
 * @details
 *  - デバイス依存はここに閉じ込め、上位は「要求(ActionRequest)」を見るだけで良い設計にするのじゃ
 *  - 先行入力バッファ、長押し/ダブルタップ、クールダウンも管理
 *  - GUIでリバインドも可能（実装は簡易／必要なければ呼ばれぬ）
 *  - 注意：std::min/std::max は使わぬ・時間変数名は dt に統一
 */
class InputActionComponent final : public Component
{
public:
    //==================== 基本情報 ====================
    /// @brief コンポーネント名を返すのじゃ
    const char* GetName() const override { return "InputAction"; }

    //==================== 論理アクション列挙 ====================
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

    //==================== アクション状態 ====================
    struct ActionState
    {
        bool pressed = false;              ///< 今フレームで押下
        bool released = false;             ///< 今フレームで解放
        bool held = false;                 ///< 押しっぱ

        int framesSincePressed = 1 << 28;  ///< 最後に押されてからのフレーム数
        int framesSinceReleased = 1 << 28; ///< 最後に離してからのフレーム数
        int cooldownFrames = 0;            ///< 残りクールダウン（0で行動可）

        int lastPressedFrame = -999999;    ///< 直近の押下フレーム
        int prevPressedFrame = -999999;    ///< そのひとつ前の押下フレーム
    };

    //==================== アクション設定 ====================
    struct ActionConfig
    {
        int acceptFrames = 7;     ///< 先行入力の受付窓（フレーム）
        int cooldownDefault = 0;  ///< 初期クールダウン（必要なら外部で適用）
        int longPressFrames = 15; ///< 長押し閾値
        int doubleTapGap = 10;    ///< ダブルタップ間隔（最大）
        bool enabled = true;      ///< コンテキスト無効化用
    };

    //==================== 出力パッケージ（生） ====================
    struct Output
    {
        DirectX::XMFLOAT2 move{ 0,0 };                 ///< デッドゾーン後の移動ベクトル
        ActionState actions[(int)ActionType::Count];   ///< 各アクションの状態
    };

    //==================== “要求”スナップショット ====================
    /**
     * @brief 上位ロジックが読み取る「そのフレームで実行したい要求」のまとめなのじゃ
     * @details
     *  - 基本は先行入力バッファを参照して True になる（未バッファ時は今フレーム pressed も見る）
     *  - consumeBuffered=true のときは該当アクションのバッファを消費する
     */
    struct ActionRequest
    {
        DirectX::XMFLOAT2 move{ 0,0 };   ///< 正規化移動入力
        bool attackLight = false;
        bool attackHeavy = false;
        bool dodge = false;
        bool parry = false;
        bool lockOn = false;
        bool jump = false;

        // 参考情報（必要に応じて見る）
        int frameStamp = 0;                 ///< 生成した内部フレーム
        bool wasDoubleTapDodge = false;     ///< ダブタ例
        bool wasLongPressHeavy = false;     ///< 長押し例
    };

    //==================== GamePad / Keyboard バインド ====================
    struct GamePadBinding { uint32_t heldMask = 0, downMask = 0, upMask = 0; };
    struct KeyboardBinding { int heldKeys[8] = { 0 }; int downKeys[8] = { 0 }; int upKeys[8] = { 0 }; };
    struct ActionBindSet { GamePadBinding pad; KeyboardBinding kb; bool enabled = true; };

public:
    //==================== ライフサイクル ====================
    /// @brief 初期化（内部状態・既定バインドをセットするのじゃ）
    void Start() override;

    /// @brief 1フレーム更新：生入力→論理化→先行入力積みを行うのじゃ
    /// @param dt 経過秒（固定FPSに換算し内部フレームを進める）
    void Update(float dt) override;

    /// @brief ギズモ不要。GUIは DebugGUI() / DebugGUI_Rebind() を必要時のみ呼ぶのじゃ
    void Render() override {}

    //==================== 取得／操作 ====================
    /// @brief 正規化後の出力（Moveと各アクション状態）を返すのじゃ
    const Output& GetOutput() const { return output; }

    /// @brief 先行入力を消費（受付窓内なら true）
    bool ConsumeBuffered(ActionType type, int maxAcceptFrames = -1);

    /// @brief バッファを覗き見る（消費しない）。受理可能なら true
    bool PeekBuffered(ActionType type, int maxAcceptFrames = -1) const;

    /// @brief “要求スナップショット”を構築して返す（必要ならバッファ消費）
    /// @param out 出力先
    /// @param consumeBuffered trueで先行入力を消費・falseで温存
    /// @param acceptFramesOverride 受付窓を一時上書き（<0で各ActionConfigを使用）
    void BuildActionRequest(ActionRequest& out, bool consumeBuffered = true, int acceptFramesOverride = -1);

    /// @brief アクションにクールダウンを与えるのじゃ
    void SetCooldown(ActionType type, int frames);

    /// @brief 内部フレームカウンタを返すのじゃ
    int GetFrameCounter() const { return frameCounter; }

    /// @brief 固定FPS（60推奨）。フレーム換算に使うのじゃ
    void SetFixedFps(float fps);

    /// @brief 円形デッドゾーン半径（0.0?0.95）を設定
    void SetMoveDeadzone(float r);

    //==================== リバインドAPI（プログラム側） ====================
    /// @brief GamePadボタンのビットマスク割当（held/down/up）を設定
    void SetGamePadBinding(ActionType a, uint32_t held, uint32_t down, uint32_t up);

    /// @brief Keyboardの割当（0終端配列）を設定（held/down/up それぞれ最大7キー）
    void SetKeyboardBinding(ActionType a, const int* heldKeys, const int* downKeys, const int* upKeys);

    /// @brief アクション毎の入力特性（受付窓/長押し/ダブタ/有効）を上書き
    void SetActionConfig(ActionType a, const ActionConfig& cfg);

    /// @brief 長押し判定（held かつ 継続フレーム >= 閾値）
    bool IsLongPress(ActionType a) const;

    /// @brief ダブルタップ判定（2回の押下間隔 <= 閾値）
    bool IsDoubleTap(ActionType a) const;

    //==================== デバッグGUI（任意） ====================
    /// @brief 簡易状態確認GUI（Pressed/Held/Releasedやバッファ）
    void DebugGUI();

    /// @brief リバインドGUI（GamePad/Keyboard）
    void DebugGUI_Rebind();

    /// @brief コンパクト版パネル：現在入力の要約
    void DrawCompactPanel();

    /// @brief 角固定のHUDオーバーレイ（小型常時表示）
    void DrawHudOverlay(bool* on, int corner = 1);

    /// @brief ポップアップ式リバインドダイアログを開く
    void OpenRebindPopup();

    /// @brief 毎フレ呼ぶ：ポップアップ本体
    void DrawRebindPopup();

private:
    //==================== 内部ヘルパ（std::min/max は使わぬ） ====================
    static float Length2(const DirectX::XMFLOAT2& v);
    static DirectX::XMFLOAT2 Normalize2D_Safe(const DirectX::XMFLOAT2& v);
    DirectX::XMFLOAT2 ApplyDeadzoneAndNormalize(const DirectX::XMFLOAT2& raw) const;

    /// @brief 内部ActionStateを今フレームの生状態から更新するのじゃ
    void UpdateActionState(ActionState& st, bool held, bool pressed, bool released);

    /// @brief 押下が有効（CD=0かつ enabled）なら先行入力へ積むのじゃ
    void PushBufferedIfAvailable(ActionType type, const ActionState& st);

    /// @brief 0終端の仮配列を dst[8] にコピー（最大7）
    static void CopyKeys8(int dst[8], const int* src);

    /// @brief デバイス→論理マッピング（GamePad＋Keyboard）
    void SampleDeviceAndMap();

private:
    //==================== 内部状態（生/論理） ====================
    Output output{};

    // デバイス生状態
    DirectX::XMFLOAT2 rawMove{ 0,0 };
    bool rawHeld[(int)ActionType::Count]{};
    bool rawDown[(int)ActionType::Count]{};
    bool rawUp[(int)ActionType::Count]{};

    // 設定
    ActionConfig actionConfig[(int)ActionType::Count]{};
    ActionBindSet bindTable[(int)ActionType::Count]{};

    // フレーム管理
    float frameAccumulator = 0.0f;
    int   frameCounter = 0;
    float fixedFps = 60.0f;
    float moveDeadzone = 0.15f;

    // 先行入力リングバッファ
    struct BufferedAction { ActionType type; int frameStamp; };
    static constexpr int BufferCap = 32;
    BufferedAction buffer[BufferCap]{};
    int bufferHead = 0;     ///< 先頭インデックス
    int bufferCount = 0;    ///< 要素数

    // Keyboard監視
    bool keyPrev[256]{};
    bool keyCurr[256]{};
    int  lastKeyboardVk = 0;

    // リバインド用（簡易）
    enum class ListenDevice { None, GamePad, Keyboard };
    enum class ListenEdge { Held, Down, Up };
    bool guiOpenRebind = false;
    ListenDevice listenDev = ListenDevice::None;
    ListenEdge   listenEdge = ListenEdge::Down;
    int  listenActionIndex = -1;
    int  listenSlotIndex = -1;
    uint32_t lastGpDownMask = 0;
};

//==================== インライン補助 ====================
inline void InputActionComponent::SetActionConfig(ActionType a, const ActionConfig& cfg) { actionConfig[(int)a] = cfg; }
inline bool  InputActionComponent::IsLongPress(ActionType a) const { const ActionState& st = output.actions[(int)a]; return (st.held && (st.framesSincePressed >= actionConfig[(int)a].longPressFrames)); }
inline bool  InputActionComponent::IsDoubleTap(ActionType a) const { const ActionState& st = output.actions[(int)a]; int gap = st.lastPressedFrame - st.prevPressedFrame; return (gap >= 0 && gap <= actionConfig[(int)a].doubleTapGap); }
