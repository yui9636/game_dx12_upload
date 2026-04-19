#pragma once

#include <cstdint>
#include <vector>

#include "PlayerEditor/TimelineAsset.h"

struct TimelineLibraryComponent
{
    std::vector<TimelineAsset> assets;
    uint32_t nextTimelineId = 1;
};
