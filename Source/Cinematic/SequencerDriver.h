#pragma once

class SequencerDriver
{
public:
    float GetTime() const { return currentTime; }
    int GetOverrideAnimationIndex() const { return overrideAnimIndex; }
    bool IsLoop() const { return isLoop; }

    void SetTime(float time) { currentTime = time; }
    void SetOverrideAnimation(int index) { overrideAnimIndex = index; }
    void SetLoop(bool loop) { isLoop = loop; }

    void Connect() {}
    void Disconnect() {}

private:
    float currentTime = 0.0f;
    int overrideAnimIndex = -1;
    bool isLoop = true;
};
