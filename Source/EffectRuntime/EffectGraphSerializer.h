#pragma once

#include <string>
#include "EffectGraphAsset.h"
// EffectGraphSerializer は編集データを保存形式へ変換し、読み込み時に同じ状態へ復元する。

class EffectGraphSerializer
{
public:
    static bool Save(const std::string& path, const EffectGraphAsset& asset);
    static bool Load(const std::string& path, EffectGraphAsset& outAsset);
};
