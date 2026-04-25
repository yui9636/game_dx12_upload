#pragma once

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <string>
#include <system_error>
#include <vector>

// Internal helpers shared between EffectEditorPanel.cpp and its extracted
// sibling modules (EffectEditorTemplates.cpp / EffectEditorAssetPicker.cpp etc.).
// Authoring helpers operate on EffectGraphAsset directly.

#include <cstdint>
#include <imgui.h>
#include "Icon/IconsFontAwesome7.h"
#include "EffectRuntime/EffectGraphAsset.h"

struct EffectGraphAsset;

namespace EffectEditorInternal
{
    inline constexpr const char* kGraphWindowTitle    = ICON_FA_DIAGRAM_PROJECT " System Overview##EffectEditor";
    inline constexpr const char* kPreviewWindowTitle  = ICON_FA_CUBE          " Preview##EffectEditor";
    inline constexpr const char* kTimelineWindowTitle = ICON_FA_CLOCK         " Timeline##EffectEditor";
    inline constexpr const char* kDetailsWindowTitle  = ICON_FA_SLIDERS       " Inspector##EffectEditor";
    inline constexpr const char* kAssetWindowTitle    = "Blackboard##EffectEditor";

    inline constexpr uint32_t kSystemVisualNodeId  = 0x70000001u;
    inline constexpr uint32_t kEmitterVisualNodeId = 0x70000002u;

    void SanitizeGraphAsset(EffectGraphAsset& asset);
    void LogGraphStructure(const EffectGraphAsset& asset, const char* tag);
    void EnsureGuiAuthoringLinks(EffectGraphAsset& asset);

    inline ImColor GetValueTypeColor(EffectValueType type)
    {
        switch (type) {
        case EffectValueType::Flow:     return ImColor(238, 181, 58);
        case EffectValueType::Float:    return ImColor(104, 178, 255);
        case EffectValueType::Vec3:     return ImColor(109, 215, 179);
        case EffectValueType::Color:    return ImColor(232, 120, 120);
        case EffectValueType::Mesh:     return ImColor(208, 156, 94);
        case EffectValueType::Particle: return ImColor(126, 208, 95);
        default:                        return ImColor(140, 140, 140);
        }
    }

    inline ImVec4 NiagaraSectionColor(EffectGraphNodeType type)
    {
        switch (type) {
        case EffectGraphNodeType::Spawn:           return ImVec4(0.19f, 0.64f, 0.89f, 1.0f);
        case EffectGraphNodeType::Lifetime:        return ImVec4(0.31f, 0.82f, 0.39f, 1.0f);
        case EffectGraphNodeType::ParticleEmitter: return ImVec4(0.97f, 0.48f, 0.11f, 1.0f);
        case EffectGraphNodeType::SpriteRenderer:
        case EffectGraphNodeType::MeshRenderer:    return ImVec4(0.98f, 0.36f, 0.23f, 1.0f);
        case EffectGraphNodeType::Color:           return ImVec4(0.17f, 0.48f, 0.98f, 1.0f);
        case EffectGraphNodeType::MeshSource:      return ImVec4(0.51f, 0.80f, 0.42f, 1.0f);
        default:                                   return ImVec4(0.55f, 0.55f, 0.60f, 1.0f);
        }
    }

    // ---- Path / asset utilities (inline so no separate .cpp needed) ----

    inline bool HasModelExtension(const std::filesystem::path& path)
    {
        std::string ext = path.extension().string();
        std::transform(ext.begin(), ext.end(), ext.begin(),
            [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        return ext == ".fbx" || ext == ".obj" || ext == ".gltf" || ext == ".glb";
    }

    inline bool HasTextureExtension(const std::filesystem::path& path)
    {
        std::string ext = path.extension().string();
        std::transform(ext.begin(), ext.end(), ext.begin(),
            [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        return ext == ".png" || ext == ".dds" || ext == ".tga" || ext == ".jpg" || ext == ".jpeg" || ext == ".bmp";
    }

    inline std::string ToLowerCopy(std::string value)
    {
        std::transform(value.begin(), value.end(), value.begin(),
            [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        return value;
    }

    inline bool ContainsInsensitive(const std::string& value, const char* needle)
    {
        if (!needle || needle[0] == '\0') {
            return true;
        }
        return ToLowerCopy(value).find(ToLowerCopy(needle)) != std::string::npos;
    }

    inline void PushRecentPath(std::vector<std::string>& paths, const std::string& path, size_t maxCount = 10)
    {
        if (path.empty()) {
            return;
        }
        paths.erase(std::remove(paths.begin(), paths.end(), path), paths.end());
        paths.insert(paths.begin(), path);
        if (paths.size() > maxCount) {
            paths.resize(maxCount);
        }
    }

    inline bool ContainsExactPath(const std::vector<std::string>& paths, const std::string& path)
    {
        return std::find(paths.begin(), paths.end(), path) != paths.end();
    }

    inline void TogglePath(std::vector<std::string>& paths, const std::string& path)
    {
        if (path.empty()) {
            return;
        }
        auto it = std::find(paths.begin(), paths.end(), path);
        if (it != paths.end()) {
            paths.erase(it);
            return;
        }
        paths.push_back(path);
    }

    inline std::vector<std::filesystem::path> CollectAssets(const std::filesystem::path& root, bool wantModels)
    {
        std::vector<std::filesystem::path> result;
        std::error_code ec;
        if (!std::filesystem::exists(root, ec)) {
            return result;
        }
        for (std::filesystem::recursive_directory_iterator it(root, ec), end; it != end; it.increment(ec)) {
            if (ec) { ec.clear(); continue; }
            if (!it->is_regular_file(ec)) continue;
            const auto& path = it->path();
            if (wantModels ? HasModelExtension(path) : HasTextureExtension(path)) {
                result.push_back(path);
            }
        }
        std::sort(result.begin(), result.end());
        return result;
    }
}
