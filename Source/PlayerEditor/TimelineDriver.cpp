#include "TimelineDriver.h"
#include "Animator/AnimatorComponent.h"

void TimelineDriver::Connect(AnimatorComponent* target)
{
    if (target) {
        m_target = target;
        target->SetDriver(this);
    }
}

void TimelineDriver::Disconnect()
{
    if (m_target) {
        m_target->SetDriver(nullptr);
        m_target = nullptr;
    }
}
