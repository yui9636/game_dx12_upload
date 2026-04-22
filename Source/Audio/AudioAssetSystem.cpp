#include "AudioAssetSystem.h"

#include <algorithm>
#include <filesystem>
#include <system_error>

#include "../../External/miniaudio-master/miniaudio.h"
#include "System/PathResolver.h"

namespace
{
    constexpr uint64_t kStreamingThresholdBytes = 2ull * 1024ull * 1024ull;

    std::filesystem::path ResolveAudioFilesystemPath(const std::string& clipPath)
    {
        if (clipPath.empty()) {
            return {};
        }

        std::filesystem::path path = std::filesystem::path(clipPath).lexically_normal();
        if (path.is_absolute()) {
            return path;
        }

        const std::string resolved = PathResolver::Resolve(clipPath);
        if (!resolved.empty()) {
            return std::filesystem::path(resolved).lexically_normal();
        }

        std::error_code ec;
        return (std::filesystem::current_path(ec) / path).lexically_normal();
    }

    std::string NormalizeAudioClipPath(const std::string& clipPath)
    {
        if (clipPath.empty()) {
            return {};
        }

        std::error_code ec;
        std::filesystem::path path = ResolveAudioFilesystemPath(clipPath);
        if (path.empty()) {
            return {};
        }

        const std::filesystem::path canonical = std::filesystem::weakly_canonical(path, ec);
        return ec ? path.string() : canonical.string();
    }

    std::string ToLower(std::string value)
    {
        std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
            return static_cast<char>(std::tolower(c));
        });
        return value;
    }
}

bool AudioAssetSystem::IsSupportedClipPath(const std::string& clipPath) const
{
    const std::string ext = ToLower(std::filesystem::path(clipPath).extension().string());
    return ext == ".wav" || ext == ".ogg" || ext == ".mp3" || ext == ".flac";
}

const AudioClipAsset* AudioAssetSystem::GetClip(const std::string& clipPath)
{
    const std::string normalizedPath = NormalizeAudioClipPath(clipPath);
    if (normalizedPath.empty() || !IsSupportedClipPath(normalizedPath)) {
        return nullptr;
    }

    const auto it = m_clipCache.find(normalizedPath);
    if (it != m_clipCache.end()) {
        return &it->second;
    }

    auto [insertIt, inserted] = m_clipCache.emplace(normalizedPath, BuildClipMetadata(normalizedPath));
    (void)inserted;
    return &insertIt->second;
}

AudioClipAsset AudioAssetSystem::GetClipOrDefault(const std::string& clipPath)
{
    if (const AudioClipAsset* clip = GetClip(clipPath)) {
        return *clip;
    }

    AudioClipAsset fallback;
    fallback.sourcePath = NormalizeAudioClipPath(clipPath);
    fallback.importedPath = fallback.sourcePath;
    return fallback;
}

void AudioAssetSystem::ClearCache()
{
    m_clipCache.clear();
}

size_t AudioAssetSystem::GetCachedClipCount() const
{
    return m_clipCache.size();
}

std::vector<AudioClipAsset> AudioAssetSystem::GetCachedClips() const
{
    std::vector<AudioClipAsset> clips;
    clips.reserve(m_clipCache.size());
    for (const auto& entry : m_clipCache) {
        clips.push_back(entry.second);
    }

    std::sort(clips.begin(), clips.end(), [](const AudioClipAsset& a, const AudioClipAsset& b) {
        return a.sourcePath < b.sourcePath;
    });
    return clips;
}

AudioClipAsset AudioAssetSystem::BuildClipMetadata(const std::string& clipPath) const
{
    AudioClipAsset clip;
    clip.sourcePath = clipPath;
    clip.importedPath = clipPath;

    std::error_code ec;
    if (!std::filesystem::exists(clipPath, ec) || ec) {
        return clip;
    }

    clip.fileSizeBytes = static_cast<uint64_t>(std::filesystem::file_size(clipPath, ec));
    if (ec) {
        clip.fileSizeBytes = 0;
    }
    clip.streaming = clip.fileSizeBytes >= kStreamingThresholdBytes;

    ma_decoder_config config = ma_decoder_config_init(ma_format_f32, 0, 0);
    ma_decoder decoder{};
    if (ma_decoder_init_file(clipPath.c_str(), &config, &decoder) != MA_SUCCESS) {
        return clip;
    }

    ma_format format = ma_format_unknown;
    ma_uint32 channels = 0;
    ma_uint32 sampleRate = 0;
    ma_decoder_get_data_format(&decoder, &format, &channels, &sampleRate, nullptr, 0);
    clip.channelCount = channels;
    clip.sampleRate = sampleRate;

    ma_uint64 lengthInFrames = 0;
    if (ma_decoder_get_length_in_pcm_frames(&decoder, &lengthInFrames) == MA_SUCCESS && sampleRate > 0) {
        clip.lengthSec = static_cast<float>(static_cast<double>(lengthInFrames) / static_cast<double>(sampleRate));
    }

    clip.valid = true;
    ma_decoder_uninit(&decoder);
    return clip;
}
