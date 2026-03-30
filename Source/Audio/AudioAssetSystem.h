#pragma once

#include <string>
#include <unordered_map>
#include <vector>

#include "Audio/AudioClipAsset.h"

class AudioAssetSystem
{
public:
    bool IsSupportedClipPath(const std::string& clipPath) const;
    const AudioClipAsset* GetClip(const std::string& clipPath);
    AudioClipAsset GetClipOrDefault(const std::string& clipPath);

    void ClearCache();
    size_t GetCachedClipCount() const;
    std::vector<AudioClipAsset> GetCachedClips() const;

private:
    AudioClipAsset BuildClipMetadata(const std::string& clipPath) const;
    std::unordered_map<std::string, AudioClipAsset> m_clipCache;
};
