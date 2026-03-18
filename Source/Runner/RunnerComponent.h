#pragma once
#include "Component/Component.h"
#include <functional>
#include <vector>


class RunnerComponent : public Component
{
public:
    // コンポーネント名（インスペクタ見出し用）
    const char* GetName() const override { return "Runner"; }

    // 開始処理：特に無し（既定値のまま開始）
    void Start() override {}

    // 毎フレーム更新：再生中なら経過時間を進め、ループ/クランプを行う
    void Update(float dt) override;

    // GUI：必要なら簡易デバッグ（今回は何もしない）
    void OnGUI() override {}

    // ───────── 既存API（変更禁止）─────────

    // クリップ長(秒)を設定（エディタやアニメ切替で更新）
    void SetClipLength(float seconds);

    // 0.0f～clipLength にクランプした時刻を直接設定（スクラブ時など）
    void SetTimeSeconds(float seconds);

    // 現在時刻(秒)取得
    float GetTimeSeconds() const { return currentSeconds; }

    // 0～1の正規化時刻（クリップ長0の時は0）取得
    float GetTimeNormalized() const;

    // 再生フラグの切替
    void Play() { playing = true; }
    void Pause() { playing = false; }
    void Stop(); // 停止して時刻を0に戻す

    // ループON/OFF
    void SetLoop(bool v) { looping = v; }
    bool IsLoop() const { return looping; }

    // 再生速度（時刻進行倍率）設定/取得
    void SetPlaySpeed(float s) { playSpeed = (s < 0.0f && !allowReversePlay) ? 0.0f : s; }
    float GetPlaySpeed() const { return playSpeed; }

    float GetClipLength() const { return clipLength; }

    /// <summary>現在時刻(秒)を取得（Sequencer互換名）</summary>
    float GetCurrentSeconds() const { return GetTimeSeconds(); }

    /// <summary>現在時刻(秒)を設定（Sequencer互換名・0..clipLengthに丸め）</summary>
    void SetCurrentSeconds(float seconds) { SetTimeSeconds(seconds); }

    /// <summary>現在の再生状態を取得（UI表示・トグル用）</summary>
    bool IsPlaying() const { return playing; }

    /// <summary>サンプリング用の安全時刻（末尾-ε補正）</summary>
    float GetTimeSecondsForSampling() const;

    // ───────── 追加API：サブレンジ再生─────────

    /// @brief 再生範囲を設定（startSeconds～endSeconds の区間のみ対象）
    /// @param startSeconds 範囲開始（負は0に丸め）
    /// @param endSeconds   範囲終了（開始より小さい場合は入れ替え）
    /// @param loopWithinRange trueで範囲内ループ／falseで範囲クランプ
    void SetPlayRange(float startSeconds, float endSeconds, bool loopWithinRange);

    /// @brief 再生範囲を解除（全区間を対象に戻す）
    void ClearPlayRange();

    /// @brief 再生範囲が有効か
    bool HasPlayRange() const { return playRangeEnabled; }

    /// @brief 範囲開始/終了（範囲無効時は0/clipLengthを返す）
    float GetRangeStartSeconds() const;
    float GetRangeEndSeconds() const;

    // ───────── 追加API：終端自動停止＆完了イベント─────────

    /// @brief 非ループ終端（または非ループ範囲終端）で自動停止するか
    void SetStopAtEnd(bool enable) { stopAtEnd = enable; }
    bool GetStopAtEnd() const { return stopAtEnd; }

    /// @brief 停止時に一度だけ呼ばれるコールバックを登録
    void SetOnFinished(std::function<void()> callback) { onFinished = callback; }

    // ───────── 追加API：逆再生の任意許可─────────

    /// @brief 逆再生（負の再生速度）を許可するか（既定false）
    void SetAllowReversePlay(bool enable) { allowReversePlay = enable; }
    bool IsReversePlayAllowed() const { return allowReversePlay; }

    // ───────── 追加API：ユーティリティ─────────

    /// @brief 終端（または範囲終端）までの残り秒（負にはならない）
    float GetRemainingSeconds() const;

    /// @brief サンプリング用εのスケール（既定1.0）
    void SetSamplingEpsilonScale(float scale)
    {
        if (scale <= 0.0f) scale = 1.0f;
        samplingEpsilonScale = scale;
    }

    // ───────── 追加API：Speedカーブ（点列・折れ線補間）─────────
    struct CurvePoint { float t01; float value; };

    /// @brief Speedカーブを有効化/無効化（無効時は常に1.0倍率）
    void SetSpeedCurveEnabled(bool enable) { speedCurveEnabled = enable; }

    /// @brief Speedカーブの正規化基準を「範囲内」にするか（true: 範囲内t01、false: クリップ全体t01）
    void SetSpeedCurveUseRangeSpace(bool enable) { speedCurveUseRangeSpace = enable; }

    // ★追加: 現在の設定状態を取得 (TimelineSequencerComponent連携用)
    bool IsSpeedCurveUseRangeSpace() const { return speedCurveUseRangeSpace; }

    /// @brief Speedカーブの点列を設定（x=t01[0..1]、y=倍率。xは昇順でない場合は内部で整列）
    void SetSpeedCurvePoints(const std::vector<CurvePoint>& points);

    /// @brief Speedカーブ点列を消去（デフォルト状態に戻す＝無点）
    void ClearSpeedCurvePoints();

    /// @brief Speedカーブが有効か
    bool IsSpeedCurveEnabled() const { return speedCurveEnabled; }

    /// @brief 現在設定されているSpeedカーブ点列を取得（参照用）
    const std::vector<CurvePoint>& GetSpeedCurvePoints() const { return speedCurvePoints; }


    void SetClipIdentity(int clipIndex);

    int GetClipIdentity() const { return clipIdentity; }

    void RequestHitStop(float duration, float speedScale);

    bool IsInHitStop() const { return hitStopTimer > 0.0f; }
private:
    // 既存フィールド
    bool  playing = false;
    bool  looping = true;
    float playSpeed = 1.0f;     // 時刻の進み倍率（カーブ適用前の基礎倍率）
    float clipLength = 0.0f;    // クリップ長(秒)
    float currentSeconds = 0.0f;// 現在時刻(秒)

    // 追加：サブレンジ
    bool  playRangeEnabled = false;
    float rangeStartSeconds = 0.0f;
    float rangeEndSeconds = 0.0f;
    bool  loopWithinRange = false;

    // 追加：終端停止＆完了コールバック
    bool  stopAtEnd = false;
    std::function<void()> onFinished;
    bool  finishedEventFiredThisTick = false;

    // 追加：逆再生許可
    bool  allowReversePlay = false;

    // 追加：サンプリングεスケール
    float samplingEpsilonScale = 1.0f;

    // 追加：Speedカーブ
    bool  speedCurveEnabled = false;
    bool  speedCurveUseRangeSpace = false;
    std::vector<CurvePoint> speedCurvePoints;

    //ヒットストップ管理用
    float hitStopTimer = 0.0f;      // 残り時間
    float hitStopSpeedScale = 0.0f; // ヒットストップ中の速度倍率

    // 追加メンバ：クリップ識別子 
    int clipIdentity = -1;  // -1=未設定。外部のアニメID連携用
private:
    // 補助：範囲を正規化（start<=end、かつ 0..clipLength）
    void NormalizeRangeToClipLength();

    // 補助：範囲適用のClamp/Wrap と 終端到達検知
    void ApplyRangeAdvance(float previousSeconds, float& nowSeconds,
        bool& reachedTerminalForward, bool& reachedTerminalBackward);

    // 補助：Speedカーブ評価（t01→倍率）。無点なら1.0を返す
    float EvaluateSpeedCurve01(float t01) const;

    // 補助：点列のx昇順整列（単純バブルでOK）
    void SortSpeedCurvePointsByT();
};