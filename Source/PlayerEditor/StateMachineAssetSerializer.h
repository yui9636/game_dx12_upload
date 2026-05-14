#pragma once
#include <string>

#include <nlohmann/json.hpp>
// StateMachineAsset はエディターで編集した内容を保存するためのデータ構造を表す。

struct StateMachineAsset;

namespace StateMachineAssetSerializer
{
    nlohmann::json ToJson(const StateMachineAsset& asset);
    bool FromJson(const nlohmann::json& root, StateMachineAsset& outAsset);
    bool Save(const std::string& path, const StateMachineAsset& asset);
    bool Load(const std::string& path, StateMachineAsset& outAsset);
}
