#pragma once

#include <string>

struct CinematicSequenceAsset;

class CinematicSequenceSerializer
{
public:
    static bool Save(const std::string& path, const CinematicSequenceAsset& asset);
    static bool Load(const std::string& path, CinematicSequenceAsset& outAsset);
};
