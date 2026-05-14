#pragma once
#include "TimelineDriver.h"
#include "TimelineAsset.h"
#include "Gameplay/AnimatorComponent.h"
#include <memory>
#include "Entity/Entity.h"
// タイムラインのスクラブ中にプレビューモードへ入る処理と戻す処理を管理する。
// ECS の Animator 再生状態を退避し、プレビュー終了時に復元する。
class PreviewState
{
public:
    bool IsActive() const { return m_active; }

    void EnterPreview(EntityID entity);
    void ExitPreview();

    // タイムライン上の時刻をアニメーション再生へ反映する。
    void SetTime(float seconds);
    void SetAnimationIndex(int index);
    void SetLoop(bool loop);

    // Play モード中に dt 分だけプレビュー再生を進める。
    void AdvanceTime(float dt, const TimelineAsset& asset);

    TimelineDriver* GetDriver() { return &m_driver; }

    // タイムライン表示用に現在フレームを返す。
    int GetCurrentFrame(float fps) const;

private:
    bool m_active = false;
    TimelineDriver m_driver;
    EntityID m_entity = Entity::NULL_ID;

    // プレビュー終了時に戻すための退避状態。
    struct SavedState
    {
        AnimatorComponent animator{};
        bool hadAnimator = false;
    };
    SavedState m_saved{};
};
