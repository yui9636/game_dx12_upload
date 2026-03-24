#include "RunnerComponent.h"
#include <algorithm>
#include <cmath>

//========================================================
//========================================================
void RunnerComponent::NormalizeRangeToClipLength()
{
    if (clipLength < 0.0f) clipLength = 0.0f;

    if (rangeStartSeconds < 0.0f) rangeStartSeconds = 0.0f;
    if (rangeEndSeconds < 0.0f) rangeEndSeconds = 0.0f;

    if (clipLength > 0.0f)
    {
        if (rangeStartSeconds > clipLength) rangeStartSeconds = clipLength;
        if (rangeEndSeconds > clipLength) rangeEndSeconds = clipLength;
    }
    else
    {
        rangeStartSeconds = 0.0f;
        rangeEndSeconds = 0.0f;
    }

    if (rangeEndSeconds < rangeStartSeconds)
    {
        float t = rangeStartSeconds;
        rangeStartSeconds = rangeEndSeconds;
        rangeEndSeconds = t;
    }
}

//========================================================
//========================================================
void RunnerComponent::SetPlayRange(float startSeconds, float endSeconds, bool loopWithin)
{
    playRangeEnabled = true;
    rangeStartSeconds = startSeconds;
    rangeEndSeconds = endSeconds;
    loopWithinRange = loopWithin;

    NormalizeRangeToClipLength();

    if (clipLength > 0.0f)
    {
        if (currentSeconds < rangeStartSeconds) currentSeconds = rangeStartSeconds;
        if (currentSeconds > rangeEndSeconds)   currentSeconds = rangeEndSeconds;
    }
    else
    {
        currentSeconds = 0.0f;
    }
}

void RunnerComponent::ClearPlayRange()
{
    playRangeEnabled = false;
    rangeStartSeconds = 0.0f;
    rangeEndSeconds = clipLength;
    loopWithinRange = false;
}

//========================================================
//========================================================
float RunnerComponent::GetRangeStartSeconds() const
{
    if (!playRangeEnabled) return 0.0f;
    return rangeStartSeconds;
}
float RunnerComponent::GetRangeEndSeconds() const
{
    if (!playRangeEnabled) return clipLength;
    return rangeEndSeconds;
}

//========================================================
//========================================================
void RunnerComponent::ApplyRangeAdvance(float previousSeconds, float& nowSeconds,
    bool& reachedTerminalForward, bool& reachedTerminalBackward)
{
    reachedTerminalForward = false;
    reachedTerminalBackward = false;

    if (clipLength <= 0.0f)
    {
        nowSeconds = 0.0f;
        return;
    }

    if (!playRangeEnabled)
    {
        if (looping)
        {
            while (nowSeconds >= clipLength) nowSeconds -= clipLength;
            while (nowSeconds < 0.0f)        nowSeconds += clipLength;
        }
        else
        {
            if (nowSeconds < 0.0f) { nowSeconds = 0.0f; }
            if (nowSeconds > clipLength) { nowSeconds = clipLength; }

            if (previousSeconds < clipLength && nowSeconds >= clipLength) reachedTerminalForward = true;
            if (allowReversePlay && previousSeconds > 0.0f && nowSeconds <= 0.0f) reachedTerminalBackward = true;
        }
        return;
    }

    float startSeconds = rangeStartSeconds;
    float endSeconds = rangeEndSeconds;

    if (endSeconds <= startSeconds)
    {
        nowSeconds = startSeconds;
        if (previousSeconds < startSeconds && nowSeconds >= startSeconds) reachedTerminalForward = true;
        if (allowReversePlay && previousSeconds > startSeconds && nowSeconds <= startSeconds) reachedTerminalBackward = true;
        return;
    }

    if (loopWithinRange)
    {
        float width = endSeconds - startSeconds;
        nowSeconds = nowSeconds - startSeconds;
        while (nowSeconds >= width) nowSeconds -= width;
        while (nowSeconds < 0.0f)   nowSeconds += width;
        nowSeconds = nowSeconds + startSeconds;
    }
    else
    {
        if (nowSeconds < startSeconds) nowSeconds = startSeconds;
        if (nowSeconds > endSeconds)   nowSeconds = endSeconds;

        if (previousSeconds < endSeconds && nowSeconds >= endSeconds) reachedTerminalForward = true;
        if (allowReversePlay && previousSeconds > startSeconds && nowSeconds <= startSeconds) reachedTerminalBackward = true;
    }
}

//========================================================
//========================================================
void RunnerComponent::SortSpeedCurvePointsByT()
{
    if (speedCurvePoints.empty()) return;
    for (int i = 0; i < (int)speedCurvePoints.size(); ++i)
    {
        for (int j = 1; j < (int)speedCurvePoints.size(); ++j)
        {
            if (speedCurvePoints[j - 1].t01 > speedCurvePoints[j].t01)
            {
                CurvePoint t = speedCurvePoints[j - 1];
                speedCurvePoints[j - 1] = speedCurvePoints[j];
                speedCurvePoints[j] = t;
            }
        }
    }
}

//========================================================
//========================================================
void RunnerComponent::SetSpeedCurvePoints(const std::vector<CurvePoint>& points)
{
    speedCurvePoints = points;

    for (auto& p : speedCurvePoints)
    {
        if (p.t01 < 0.0f) p.t01 = 0.0f;
        if (p.t01 > 1.0f) p.t01 = 1.0f;
        if (p.value < 0.0f) p.value = 0.0f;
        if (p.value > 100.0f) p.value = 100.0f;
    }

    SortSpeedCurvePointsByT();
}

void RunnerComponent::ClearSpeedCurvePoints()
{
    speedCurvePoints.clear();
}

//========================================================
//========================================================
float RunnerComponent::EvaluateSpeedCurve01(float t01) const
{
    if (!speedCurveEnabled) return 1.0f;
    if (speedCurvePoints.empty()) return 1.0f;

    // Clamp
    if (t01 < 0.0f) t01 = 0.0f;
    if (t01 > 1.0f) t01 = 1.0f;

    if (t01 <= speedCurvePoints.front().t01) return speedCurvePoints.front().value;
    if (t01 >= speedCurvePoints.back().t01)  return speedCurvePoints.back().value;

    for (int i = 1; i < (int)speedCurvePoints.size(); ++i)
    {
        const CurvePoint& a = speedCurvePoints[i - 1];
        const CurvePoint& b = speedCurvePoints[i];
        if (t01 >= a.t01 && t01 <= b.t01)
        {
            float w = 0.0f;
            float dx = (b.t01 - a.t01);
            if (dx > 0.0f) {
                w = (t01 - a.t01) / dx;
                if (w < 0.0f) w = 0.0f;
                if (w > 1.0f) w = 1.0f;
            }
            float v = a.value + (b.value - a.value) * w;

            if (v < 0.0f) v = 0.0f;
            if (v > 100.0f) v = 100.0f;
            return v;
        }
    }
    return speedCurvePoints.back().value;
}

//========================================================
//========================================================

void RunnerComponent::RequestHitStop(float duration, float speedScale)
{
    if (duration > 0.0f)
    {
        hitStopTimer = duration;
        hitStopSpeedScale = speedScale;
    }
}

//void RunnerComponent::Update(float dt)
//{
//    finishedEventFiredThisTick = false;
//
//    if (!playing) return;
//    if (clipLength <= 0.0f) return;
//
//    float t01 = 0.0f;
//    if (playRangeEnabled && speedCurveUseRangeSpace)
//    {
//        float startSeconds = GetRangeStartSeconds();
//        float endSeconds = GetRangeEndSeconds();
//        float widthSeconds = endSeconds - startSeconds;
//        if (widthSeconds <= 0.0f) {
//            t01 = 0.0f;
//        }
//        else {
//            float local = currentSeconds - startSeconds;
//            if (local < 0.0f) local = 0.0f;
//            if (local > widthSeconds) local = widthSeconds;
//            t01 = local / widthSeconds;
//            if (t01 < 0.0f) t01 = 0.0f;
//            if (t01 > 1.0f) t01 = 1.0f;
//        }
//    }
//    else
//    {
//        if (clipLength > 0.0f) {
//            t01 = currentSeconds / clipLength;
//            if (t01 < 0.0f) t01 = 0.0f;
//            if (t01 > 1.0f) t01 = 1.0f;
//        }
//    }
//
//    float speedMultiplier = EvaluateSpeedCurve01(t01);
//    float effectivePlaySpeed = playSpeed * speedMultiplier;
//
//    if (!allowReversePlay && effectivePlaySpeed < 0.0f) effectivePlaySpeed = 0.0f;
//
//    float deltaSeconds = dt * effectivePlaySpeed;
//
//    float previousSeconds = currentSeconds;
//    float nowSeconds = currentSeconds + deltaSeconds;
//
//    bool reachedForward = false;
//    bool reachedBackward = false;
//    ApplyRangeAdvance(previousSeconds, nowSeconds, reachedForward, reachedBackward);
//
//    currentSeconds = nowSeconds;
//
//    bool nonLoopingNow = (!playRangeEnabled && !looping) || (playRangeEnabled && !loopWithinRange);
//    if (stopAtEnd && nonLoopingNow)
//    {
//        if (reachedForward || reachedBackward)
//        {
//            playing = false;
//            if (onFinished)
//            {
//                onFinished();
//                finishedEventFiredThisTick = true;
//            }
//        }
//    }
//}
void RunnerComponent::Update(float dt)
{
    finishedEventFiredThisTick = false;

    if (!playing) return;
    if (clipLength <= 0.0f) return;

    // ----------------------------------------------------
    // ----------------------------------------------------
    float finalTimeScale = 1.0f;

    if (hitStopTimer > 0.0f)
    {
        finalTimeScale = hitStopSpeedScale;

        hitStopTimer -= dt;
        if (hitStopTimer < 0.0f) hitStopTimer = 0.0f;
    }
    else
    {

        float t01 = 0.0f;
        if (playRangeEnabled && speedCurveUseRangeSpace)
        {
            float startSeconds = GetRangeStartSeconds();
            float endSeconds = GetRangeEndSeconds();
            float widthSeconds = endSeconds - startSeconds;
            if (widthSeconds <= 0.0f) {
                t01 = 0.0f;
            }
            else {
                float local = currentSeconds - startSeconds;
                if (local < 0.0f) local = 0.0f;
                if (local > widthSeconds) local = widthSeconds;
                t01 = local / widthSeconds;
                if (t01 < 0.0f) t01 = 0.0f;
                if (t01 > 1.0f) t01 = 1.0f;
            }
        }
        else
        {
            if (clipLength > 0.0f) {
                t01 = currentSeconds / clipLength;
                if (t01 < 0.0f) t01 = 0.0f;
                if (t01 > 1.0f) t01 = 1.0f;
            }
        }

        finalTimeScale = EvaluateSpeedCurve01(t01);
    }

    // ----------------------------------------------------
    // ----------------------------------------------------

    float effectivePlaySpeed = playSpeed * finalTimeScale;

    if (!allowReversePlay && effectivePlaySpeed < 0.0f) effectivePlaySpeed = 0.0f;

    float deltaSeconds = dt * effectivePlaySpeed;

    float previousSeconds = currentSeconds;
    float nowSeconds = currentSeconds + deltaSeconds;

    bool reachedForward = false;
    bool reachedBackward = false;
    ApplyRangeAdvance(previousSeconds, nowSeconds, reachedForward, reachedBackward);

    currentSeconds = nowSeconds;

    bool nonLoopingNow = (!playRangeEnabled && !looping) || (playRangeEnabled && !loopWithinRange);
    if (stopAtEnd && nonLoopingNow)
    {
        if (reachedForward || reachedBackward)
        {
            playing = false;
            if (onFinished)
            {
                onFinished();
                finishedEventFiredThisTick = true;
            }
        }
    }
}


//========================================================
//========================================================
void RunnerComponent::SetClipLength(float seconds)
{
    if (seconds <= 0.0f) seconds = 0.0f;
    clipLength = seconds;

    if (clipLength <= 0.0f)
    {
        currentSeconds = 0.0f;
    }
    else
    {
        if (currentSeconds < 0.0f) currentSeconds = 0.0f;
        if (currentSeconds > clipLength) currentSeconds = looping ? (currentSeconds - clipLength) : clipLength;
        if (looping)
        {
            while (currentSeconds >= clipLength) currentSeconds = currentSeconds - clipLength;
            while (currentSeconds < 0.0f)       currentSeconds = currentSeconds + clipLength;
        }
    }

    if (playRangeEnabled)
    {
        NormalizeRangeToClipLength();
        if (clipLength > 0.0f)
        {
            if (currentSeconds < rangeStartSeconds) currentSeconds = rangeStartSeconds;
            if (currentSeconds > rangeEndSeconds)   currentSeconds = rangeEndSeconds;
        }
        else
        {
            currentSeconds = 0.0f;
        }
    }
}

//========================================================
//========================================================
void RunnerComponent::SetTimeSeconds(float seconds)
{
    if (clipLength <= 0.0f) {
        currentSeconds = 0.0f;
        return;
    }
    if (seconds < 0.0f) seconds = 0.0f;
    if (seconds > clipLength) seconds = clipLength;
    currentSeconds = seconds;
}

//========================================================
//========================================================
float RunnerComponent::GetTimeNormalized() const
{
    if (clipLength <= 0.0f) return 0.0f;
    float t = currentSeconds / clipLength;
    if (t < 0.0f) t = 0.0f;
    if (t > 1.0f) t = 1.0f;
    return t;
}

//========================================================
//========================================================
void RunnerComponent::Stop()
{
    playing = false;
    currentSeconds = 0.0f;
}

//========================================================
//========================================================
float RunnerComponent::GetTimeSecondsForSampling() const
{
    if (clipLength <= 0.0f) return 0.0f;

    float seconds = currentSeconds;

    float epsilon = (clipLength * 0.0001f + 1e-5f) * samplingEpsilonScale;
    if (epsilon < 0.0f) epsilon = 0.0f;

    float startForClamp = playRangeEnabled ? GetRangeStartSeconds() : 0.0f;
    float endForClamp = playRangeEnabled ? GetRangeEndSeconds() : clipLength;

    bool loopLike = (playRangeEnabled && loopWithinRange) || (!playRangeEnabled && looping);

    if (loopLike)
    {
        float endEdge = endForClamp - 0.5f * epsilon;
        if (seconds >= endEdge) return startForClamp;
        if (seconds < startForClamp) return startForClamp;
        return seconds;
    }
    else
    {
        if (seconds >= endForClamp) seconds = endForClamp - epsilon;
        if (seconds < startForClamp) seconds = startForClamp;
        return seconds;
    }
}

//========================================================
//========================================================
float RunnerComponent::GetRemainingSeconds() const
{
    if (clipLength <= 0.0f) return 0.0f;

    float endForClamp = playRangeEnabled ? GetRangeEndSeconds() : clipLength;
    float remain = endForClamp - currentSeconds;
    if (remain < 0.0f) remain = 0.0f;
    return remain;
}


void RunnerComponent::SetClipIdentity(int clipIndex)
{
    clipIdentity = clipIndex;
}
