#pragma once

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <filesystem>
#include <string>
#include <utility>

#include "Asset/AssetManager.h"

class EditorAssetContext
{
public:
    static EditorAssetContext& Instance()
    {
        static EditorAssetContext instance;
        return instance;
    }

    void SetActiveAsset(std::string path, AssetType type = AssetType::Unknown)
    {
        m_activeAssetPath = std::move(path);
        m_activeAssetType = (type == AssetType::Unknown)
            ? GuessAssetType(m_activeAssetPath)
            : type;
        ++m_revision;
    }

    void Clear()
    {
        if (m_activeAssetPath.empty() && m_activeAssetType == AssetType::Unknown) {
            return;
        }
        m_activeAssetPath.clear();
        m_activeAssetType = AssetType::Unknown;
        ++m_revision;
    }

    const std::string& GetActiveAssetPath() const { return m_activeAssetPath; }
    AssetType GetActiveAssetType() const { return m_activeAssetType; }
    uint64_t GetRevision() const { return m_revision; }
    bool HasActiveAsset() const { return !m_activeAssetPath.empty(); }
    bool IsActiveTexture() const { return m_activeAssetType == AssetType::Texture && !m_activeAssetPath.empty(); }
    bool IsActiveFont() const { return m_activeAssetType == AssetType::Font && !m_activeAssetPath.empty(); }
    bool IsActiveModel() const { return m_activeAssetType == AssetType::Model && !m_activeAssetPath.empty(); }
    bool IsActiveAudio() const { return m_activeAssetType == AssetType::Audio && !m_activeAssetPath.empty(); }

    static AssetType GuessAssetType(const std::string& path)
    {
        std::string ext = std::filesystem::path(path).extension().string();
        std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) {
            return static_cast<char>(std::tolower(c));
        });

        if (ext == ".png" || ext == ".jpg" || ext == ".jpeg" ||
            ext == ".tga" || ext == ".dds" || ext == ".bmp" || ext == ".hdr") {
            return AssetType::Texture;
        }
        if (ext == ".ttf" || ext == ".otf" || ext == ".fnt") {
            return AssetType::Font;
        }
        if (ext == ".fbx" || ext == ".obj" || ext == ".blend" || ext == ".gltf") {
            return AssetType::Model;
        }
        if (ext == ".prefab") {
            return AssetType::Prefab;
        }
        if (ext == ".wav" || ext == ".ogg" || ext == ".mp3" || ext == ".flac") {
            return AssetType::Audio;
        }
        if (ext == ".mat") {
            return AssetType::Material;
        }
        return AssetType::Unknown;
    }

private:
    EditorAssetContext() = default;

    std::string m_activeAssetPath;
    AssetType m_activeAssetType = AssetType::Unknown;
    uint64_t m_revision = 0;
};
