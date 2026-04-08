#pragma once

#include "Entity/Entity.h"

class TimelineDriver
{
public:
    float GetTime() const { return m_currentTime; }
    bool IsLoop() const { return m_loop; }
    int GetOverrideAnimationIndex() const { return m_animIndex; }

    void SetTime(float t);
    void SetOverrideAnimation(int idx);
    void SetLoop(bool loop);

    void Connect(EntityID targetEntity);
    void Disconnect();

    ~TimelineDriver() { Disconnect(); }

private:
    void Sync();

private:
    float m_currentTime = 0.0f;
    int m_animIndex = -1;
    bool m_loop = false;
    EntityID m_targetEntity = Entity::NULL_ID;
};
