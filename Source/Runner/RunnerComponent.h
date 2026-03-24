#pragma once
#include "Component/Component.h"
#include <functional>
#include <vector>


class RunnerComponent : public Component
{
public:
    const char* GetName() const override { return "Runner"; }

    void Start() override {}

    void Update(float dt) override;

    void OnGUI() override {}


    void SetClipLength(float seconds);

    void SetTimeSeconds(float seconds);

    float GetTimeSeconds() const { return currentSeconds; }

    float GetTimeNormalized() const;

    void Play() { playing = true; }
    void Pause() { playing = false; }
    void Stop();

    void SetLoop(bool v) { looping = v; }
    bool IsLoop() const { return looping; }

    void SetPlaySpeed(float s) { playSpeed = (s < 0.0f && !allowReversePlay) ? 0.0f : s; }
    float GetPlaySpeed() const { return playSpeed; }

    float GetClipLength() const { return clipLength; }

    float GetCurrentSeconds() const { return GetTimeSeconds(); }

    void SetCurrentSeconds(float seconds) { SetTimeSeconds(seconds); }

    bool IsPlaying() const { return playing; }

    float GetTimeSecondsForSampling() const;


    void SetPlayRange(float startSeconds, float endSeconds, bool loopWithinRange);

    void ClearPlayRange();

    bool HasPlayRange() const { return playRangeEnabled; }

    float GetRangeStartSeconds() const;
    float GetRangeEndSeconds() const;


    void SetStopAtEnd(bool enable) { stopAtEnd = enable; }
    bool GetStopAtEnd() const { return stopAtEnd; }

    void SetOnFinished(std::function<void()> callback) { onFinished = callback; }


    void SetAllowReversePlay(bool enable) { allowReversePlay = enable; }
    bool IsReversePlayAllowed() const { return allowReversePlay; }


    float GetRemainingSeconds() const;

    void SetSamplingEpsilonScale(float scale)
    {
        if (scale <= 0.0f) scale = 1.0f;
        samplingEpsilonScale = scale;
    }

    struct CurvePoint { float t01; float value; };

    void SetSpeedCurveEnabled(bool enable) { speedCurveEnabled = enable; }

    void SetSpeedCurveUseRangeSpace(bool enable) { speedCurveUseRangeSpace = enable; }

    bool IsSpeedCurveUseRangeSpace() const { return speedCurveUseRangeSpace; }

    void SetSpeedCurvePoints(const std::vector<CurvePoint>& points);

    void ClearSpeedCurvePoints();

    bool IsSpeedCurveEnabled() const { return speedCurveEnabled; }

    const std::vector<CurvePoint>& GetSpeedCurvePoints() const { return speedCurvePoints; }


    void SetClipIdentity(int clipIndex);

    int GetClipIdentity() const { return clipIdentity; }

    void RequestHitStop(float duration, float speedScale);

    bool IsInHitStop() const { return hitStopTimer > 0.0f; }
private:
    bool  playing = false;
    bool  looping = true;
    float playSpeed = 1.0f;
    float clipLength = 0.0f;
    float currentSeconds = 0.0f;

    bool  playRangeEnabled = false;
    float rangeStartSeconds = 0.0f;
    float rangeEndSeconds = 0.0f;
    bool  loopWithinRange = false;

    bool  stopAtEnd = false;
    std::function<void()> onFinished;
    bool  finishedEventFiredThisTick = false;

    bool  allowReversePlay = false;

    float samplingEpsilonScale = 1.0f;

    bool  speedCurveEnabled = false;
    bool  speedCurveUseRangeSpace = false;
    std::vector<CurvePoint> speedCurvePoints;

    float hitStopTimer = 0.0f;
    float hitStopSpeedScale = 0.0f;

    int clipIdentity = -1;
private:
    void NormalizeRangeToClipLength();

    void ApplyRangeAdvance(float previousSeconds, float& nowSeconds,
        bool& reachedTerminalForward, bool& reachedTerminalBackward);

    float EvaluateSpeedCurve01(float t01) const;

    void SortSpeedCurvePointsByT();
};
