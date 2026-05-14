#pragma once
#include <string>

#include <nlohmann/json.hpp>
// TimelineAsset はエディターで編集した内容を保存するためのデータ構造を表す。

struct TimelineAsset;

namespace TimelineAssetSerializer
{
    nlohmann::json ToJson(const TimelineAsset& asset);
    bool FromJson(const nlohmann::json& root, TimelineAsset& outAsset);
    bool Save(const std::string& path, const TimelineAsset& asset);
    bool Load(const std::string& path, TimelineAsset& outAsset);
}
