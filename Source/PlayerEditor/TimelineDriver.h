#pragma once
#include "Animator/IAnimationDriver.h"

class AnimatorComponent;

class TimelineDriver : public IAnimationDriver
{
public:
    // IAnimationDriver
    float GetTime() const override { return m_currentTime; }
    bool  AllowInternalUpdate() const override { return false; }
    int   GetOverrideAnimationIndex() const override { return m_animIndex; }
    bool  IsLoop() const override { return m_loop; }

    // Setters
    void SetTime(float t)              { m_currentTime = t; }
    void SetOverrideAnimation(int idx) { m_animIndex = idx; }
    void SetLoop(bool loop)            { m_loop = loop; }

    // Connect/Disconnect to AnimatorComponent
    void Connect(AnimatorComponent* target);
    void Disconnect();

    ~TimelineDriver() { Disconnect(); }

private:
    float m_currentTime = 0.0f;
    int   m_animIndex   = -1;
    bool  m_loop        = false;

    AnimatorComponent* m_target = nullptr;
};
