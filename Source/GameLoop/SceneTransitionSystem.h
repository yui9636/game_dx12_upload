#pragma once

#include "Asset/PrefabSystem.h"

class Registry;
struct GameLoopRuntime;

class SceneTransitionSystem
{
public:
    static bool UpdateEndOfFrame(
        GameLoopRuntime& runtime,
        Registry& gameRegistry,
        SceneFileMetadata* outMetadata = nullptr);
};