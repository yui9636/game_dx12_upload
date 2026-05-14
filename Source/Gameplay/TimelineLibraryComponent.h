#pragma once

#include <cstdint>
#include <vector>

#include "PlayerEditor/TimelineAsset.h"
// TimelineLibraryComponent は assets/nextTimelineId を保持し、関連システムが実行時状態として参照する。

struct TimelineLibraryComponent
{
    std::vector<TimelineAsset> assets;
    uint32_t nextTimelineId = 1;
};
