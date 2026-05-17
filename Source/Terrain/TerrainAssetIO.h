#pragma once
#include <filesystem>

struct TerrainAsset;

// Binary serialization helpers for TerrainAsset. Used as a sidecar file
// referenced from scene/prefab JSON (height + splat data are too large to
// embed directly).
namespace TerrainAssetIO
{
    // Write the asset (incl. heightData / splatData) to a binary .terrain file.
    bool SaveToFile(const TerrainAsset& asset, const std::filesystem::path& path);

    // Restore the asset from a .terrain file. Returns false if path missing
    // or file is malformed.
    bool LoadFromFile(TerrainAsset& asset, const std::filesystem::path& path);
}
