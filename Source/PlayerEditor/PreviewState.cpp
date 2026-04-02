#include "PreviewState.h"
#include "Animator/AnimatorComponent.h"

void PreviewState::EnterPreview(AnimatorComponent* animator)
{
    if (m_active || !animator) return;

    m_animator = animator;
    m_active = true;

    // Save current state (driver is private; we just track that we entered preview)
    m_saved.hadDriver = false;

    // Connect our driver
    m_driver.Connect(animator);
    m_driver.SetLoop(false);
    m_driver.SetTime(0.0f);
}

void PreviewState::ExitPreview()
{
    if (!m_active) return;

    // Disconnect driver (restores nullptr)
    m_driver.Disconnect();

    m_animator = nullptr;
    m_active = false;
}

void PreviewState::SetTime(float seconds)
{
    m_driver.SetTime(seconds);
}

void PreviewState::SetAnimationIndex(int index)
{
    m_driver.SetOverrideAnimation(index);
}

void PreviewState::SetLoop(bool loop)
{
    m_driver.SetLoop(loop);
}

void PreviewState::AdvanceTime(float dt, const TimelineAsset& asset)
{
    if (!m_active) return;

    float t = m_driver.GetTime() + dt;
    if (asset.duration > 0.0f) {
        if (t > asset.duration) {
            if (m_driver.IsLoop())
                t = fmodf(t, asset.duration);
            else
                t = asset.duration;
        }
    }
    m_driver.SetTime(t);
}

int PreviewState::GetCurrentFrame(float fps) const
{
    return static_cast<int>(m_driver.GetTime() * fps);
}
