#include "TerrainAssetIO.h"
#include "TerrainAsset.h"
#include <cereal/archives/binary.hpp>
#include <fstream>

namespace TerrainAssetIO
{
    bool SaveToFile(const TerrainAsset& asset, const std::filesystem::path& path)
    {
        std::error_code ec;
        std::filesystem::create_directories(path.parent_path(), ec);
        std::ofstream ofs(path, std::ios::binary);
        if (!ofs.is_open()) return false;
        try {
            cereal::BinaryOutputArchive archive(ofs);
            archive(const_cast<TerrainAsset&>(asset));
        } catch (...) {
            return false;
        }
        return true;
    }

    bool LoadFromFile(TerrainAsset& asset, const std::filesystem::path& path)
    {
        std::ifstream ifs(path, std::ios::binary);
        if (!ifs.is_open()) return false;
        try {
            cereal::BinaryInputArchive archive(ifs);
            archive(asset);
        } catch (...) {
            return false;
        }
        return true;
    }
}
