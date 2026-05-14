#pragma once

#include <string>
// CinematicSequenceAsset はエディターで編集した内容を保存するためのデータ構造を表す。

struct CinematicSequenceAsset;

class CinematicSequenceSerializer
{
public:
    static bool Save(const std::string& path, const CinematicSequenceAsset& asset);
    static bool Load(const std::string& path, CinematicSequenceAsset& outAsset);
};
