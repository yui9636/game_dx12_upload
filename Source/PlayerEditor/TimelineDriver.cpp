#include "TimelineDriver.h"
#include "Animator/AnimatorService.h"

void TimelineDriver::SetTime(float t)
{
    m_currentTime = t;
    Sync();
}

void TimelineDriver::SetOverrideAnimation(int idx)
{
    m_animIndex = idx;
    Sync();
}

void TimelineDriver::SetLoop(bool loop)
{
    m_loop = loop;
    Sync();
}

void TimelineDriver::Connect(EntityID targetEntity)
{
    m_targetEntity = targetEntity;
    Sync();
}

void TimelineDriver::Disconnect()
{
    if (!Entity::IsNull(m_targetEntity)) {
        AnimatorService::Instance().ClearDriver(m_targetEntity);
        m_targetEntity = Entity::NULL_ID;
    }
}

void TimelineDriver::Sync()
{
    if (Entity::IsNull(m_targetEntity)) {
        return;
    }
    AnimatorService::Instance().SetDriver(m_targetEntity, m_currentTime, m_animIndex, m_loop, false);
}
