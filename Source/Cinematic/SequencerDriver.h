#pragma once
// シーケンサー再生状態を外部システムへ渡すための簡易ドライバー。

// 現在時刻・上書きアニメーション・ループ設定を保持する。
class SequencerDriver
{
public:
    // 現在のシーケンサー再生時間を取得する。
    float GetTime() const { return currentTime; }
    // シーケンサーが上書き再生したいアニメーション番号を取得する。
    int GetOverrideAnimationIndex() const { return overrideAnimIndex; }
    // アニメーションをループ扱いにするか取得する。
    bool IsLoop() const { return isLoop; }

    // 外部へ渡す現在時刻を設定する。
    void SetTime(float time) { currentTime = time; }
    // 上書き再生するアニメーション番号を設定する。
    void SetOverrideAnimation(int index) { overrideAnimIndex = index; }
    // ループ設定を変更する。
    void SetLoop(bool loop) { isLoop = loop; }

    // 将来の接続処理用。現在は何もしない。
    void Connect() {}
    // 将来の切断処理用。現在は何もしない。
    void Disconnect() {}

private:
    // 現在の再生時間。
    float currentTime = 0.0f;
    // 上書き再生するアニメーション番号。-1 は指定なし。
    int overrideAnimIndex = -1;
    // アニメーションをループとして扱うか。
    bool isLoop = true;
};
