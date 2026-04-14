#include "PreviewState.h"
#include "Animator/AnimatorService.h"
#include "Registry/Registry.h"
#include <cmath>

void PreviewState::EnterPreview(EntityID entity)
{
    if (m_active || Entity::IsNull(entity)) return;

    Registry* registry = AnimatorService::Instance().GetRegistry();
    if (registry && registry->IsAlive(entity)) {
        if (AnimatorComponent* animator = registry->GetComponent<AnimatorComponent>(entity)) {
            m_saved.animator = *animator;
            m_saved.hadAnimator = true;
        } else {
            m_saved.hadAnimator = false;
        }
    } else {
        m_saved.hadAnimator = false;
    }

    m_entity = entity;
    m_active = true;

    AnimatorService::Instance().EnsureAnimator(entity);
    m_driver.Connect(entity);
    m_driver.SetLoop(false);
    m_driver.SetTime(0.0f);
}

void PreviewState::ExitPreview()
{
    if (!m_active) return;

    Registry* registry = AnimatorService::Instance().GetRegistry();
    const EntityID entity = m_entity;
    m_driver.Disconnect();
    if (registry && !Entity::IsNull(entity) && registry->IsAlive(entity)) {
        if (m_saved.hadAnimator) {
            if (AnimatorComponent* animator = registry->GetComponent<AnimatorComponent>(entity)) {
                *animator = m_saved.animator;
            } else {
                registry->AddComponent<AnimatorComponent>(entity, m_saved.animator);
            }
        } else {
            AnimatorService::Instance().RemoveAnimator(entity);
        }
    }
    m_entity = Entity::NULL_ID;
    m_active = false;
    m_saved = SavedState{};
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
            if (m_driver.IsLoop()) {
                t = std::fmod(t, asset.duration);
            } else {
                t = asset.duration;
            }
        }
    }
    m_driver.SetTime(t);
}

int PreviewState::GetCurrentFrame(float fps) const
{
    return static_cast<int>(m_driver.GetTime() * fps);
}
