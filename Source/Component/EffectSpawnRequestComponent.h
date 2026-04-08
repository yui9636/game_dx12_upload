#pragma once

struct EffectSpawnRequestComponent
{
    bool pending = true;
    bool restartIfActive = true;
    int requestGeneration = 0;
    float startTime = 0.0f;
};
