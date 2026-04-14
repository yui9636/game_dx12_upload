#pragma once

#include "Asset/ModelAssetSerializer.h"

#include <filesystem>
#include <string>

class ModelSerializerPanel
{
public:
    void Draw(bool* p_open = nullptr, bool* outFocused = nullptr);

private:
    bool AcceptSourceAsset(const std::filesystem::path& path);
    static bool IsSupportedSourceAsset(const std::filesystem::path& path);
    static std::filesystem::path BuildDefaultOutputPath(const std::filesystem::path& sourcePath);

    std::filesystem::path m_sourcePath;
    std::string m_outputPath;
    ModelSerializerSettings m_settings;
    ModelSerializerResult m_lastResult;
    bool m_hasResult = false;
};
