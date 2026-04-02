#pragma once
#include <string>

struct TimelineAsset;

namespace TimelineAssetSerializer
{
    bool Save(const std::string& path, const TimelineAsset& asset);
    bool Load(const std::string& path, TimelineAsset& outAsset);
}
