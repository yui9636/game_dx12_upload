#pragma once
#include "Storage/GameplayAsset.h"
#include <vector>
// TimelineItemBuffer は items を中心に、実行時やエディターで共有する状態を保持する。

struct TimelineItemBuffer {
    std::vector<GESequencerItem> items;
};
