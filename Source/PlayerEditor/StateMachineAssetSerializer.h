#pragma once
#include <string>

struct StateMachineAsset;

namespace StateMachineAssetSerializer
{
    bool Save(const std::string& path, const StateMachineAsset& asset);
    bool Load(const std::string& path, StateMachineAsset& outAsset);
}
