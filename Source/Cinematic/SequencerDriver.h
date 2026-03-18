#pragma once
#include "Animator/IAnimationDriver.h"
#include "Animator/AnimatorComponent.h" // SetDriver呼び出し用

// シーケンサーがアクターを完全制御するためのドライバー
class SequencerDriver : public IAnimationDriver
{
public:
    // ----------------------------------------------------------
    // IAnimationDriver インターフェース実装
    // ----------------------------------------------------------

    // シーケンサーの現在時間を供給
    float GetTime() const override { return currentTime; }

    // シーケンサー操作中は、AIや物理移動を許可しない (完全停止)
    bool AllowInternalUpdate() const override { return false; }

    // 強制したいアニメーション番号 (-1なら指定なし)
    int GetOverrideAnimationIndex() const override { return overrideAnimIndex; }

    // ----------------------------------------------------------
    // シーケンサーからの操作用 API
    // ----------------------------------------------------------

    // 時間をセット（スクラブや再生中）
    void SetTime(float time) { currentTime = time; }

    // 強制するアニメーションを指定
    void SetOverrideAnimation(int index) { overrideAnimIndex = index; }

    bool IsLoop() const override { return isLoop; }

    void SetLoop(bool loop) { isLoop = loop; }

    // アクターに接続（憑依開始）
    void Connect(AnimatorComponent* target)
    {
        if (target) {
            targetAnimator = target;
            target->SetDriver(this); // ここで制御権を奪う
        }
    }

    // アクターから切断（憑依解除）
    void Disconnect()
    {
        if (targetAnimator) {
            targetAnimator->SetDriver(nullptr); // 制御権を返す
            targetAnimator = nullptr;
        }
    }

    // デストラクタで安全に切断（重要！）
    // エディターを閉じた時にクラッシュさせないための保険
    ~SequencerDriver()
    {
        Disconnect();
    }

private:
    float currentTime = 0.0f;
    int overrideAnimIndex = -1;

    bool isLoop = true;

    // 現在接続中の相手（切断用）
    AnimatorComponent* targetAnimator = nullptr;
};