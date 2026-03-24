#pragma once
#include "Animator/IAnimationDriver.h"
#include "Animator/AnimatorComponent.h"

class SequencerDriver : public IAnimationDriver
{
public:
    // ----------------------------------------------------------
    // ----------------------------------------------------------

    float GetTime() const override { return currentTime; }

    bool AllowInternalUpdate() const override { return false; }

    int GetOverrideAnimationIndex() const override { return overrideAnimIndex; }

    // ----------------------------------------------------------
    // ----------------------------------------------------------

    void SetTime(float time) { currentTime = time; }

    void SetOverrideAnimation(int index) { overrideAnimIndex = index; }

    bool IsLoop() const override { return isLoop; }

    void SetLoop(bool loop) { isLoop = loop; }

    void Connect(AnimatorComponent* target)
    {
        if (target) {
            targetAnimator = target;
            target->SetDriver(this);
        }
    }

    void Disconnect()
    {
        if (targetAnimator) {
            targetAnimator->SetDriver(nullptr);
            targetAnimator = nullptr;
        }
    }

    ~SequencerDriver()
    {
        Disconnect();
    }

private:
    float currentTime = 0.0f;
    int overrideAnimIndex = -1;

    bool isLoop = true;

    AnimatorComponent* targetAnimator = nullptr;
};
