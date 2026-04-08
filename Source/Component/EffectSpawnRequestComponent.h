#pragma once

struct EffectSpawnRequestComponent
{
    bool pending = true;
    bool restartIfActive = true;
    int requestGeneration = 0;
};
