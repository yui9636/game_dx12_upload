#pragma once
#include "TimelineDriver.h"
#include "TimelineAsset.h"
#include <memory>

class AnimatorComponent;
class Actor;

// ============================================================================
// Manages enter/exit preview mode for timeline scrubbing
// Saves and restores AnimatorComponent state
// ============================================================================

class PreviewState
{
public:
    bool IsActive() const { return m_active; }

    void EnterPreview(AnimatorComponent* animator);
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
    AnimatorComponent* m_animator = nullptr;

    // Saved state for restore
    struct SavedState
    {
        int   baseAnimIndex   = 0;
        float baseTime        = 0.0f;
        bool  baseLoop        = true;
        int   actionAnimIndex = -1;
        float actionTime      = 0.0f;
        bool  actionLoop      = false;
        bool  hadDriver       = false;
    };
    SavedState m_saved{};
};
