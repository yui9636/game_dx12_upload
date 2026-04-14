#pragma once
#include "TimelineDriver.h"
#include "TimelineAsset.h"
#include "Gameplay/AnimatorComponent.h"
#include <memory>
#include "Entity/Entity.h"

// ============================================================================
// Manages enter/exit preview mode for timeline scrubbing
// Saves and restores timeline preview state for ECS animator playback.
// ============================================================================

class PreviewState
{
public:
    bool IsActive() const { return m_active; }

    void EnterPreview(EntityID entity);
    void ExitPreview();

    // Drive animation from timeline
    void SetTime(float seconds);
    void SetAnimationIndex(int index);
    void SetLoop(bool loop);

    // Advance playback by dt (for Play mode)
    void AdvanceTime(float dt, const TimelineAsset& asset);

    TimelineDriver* GetDriver() { return &m_driver; }

    // Get current frame for timeline display
    int GetCurrentFrame(float fps) const;

private:
    bool m_active = false;
    TimelineDriver m_driver;
    EntityID m_entity = Entity::NULL_ID;

    // Saved state for restore
    struct SavedState
    {
        AnimatorComponent animator{};
        bool hadAnimator = false;
    };
    SavedState m_saved{};
};
