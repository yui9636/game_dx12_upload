#include "PathResolver.h"
#include <windows.h>
#include <algorithm>
#include <filesystem>
#include <system_error>
#include <vector>

std::string PathResolver::s_RootPath = "";

namespace
{
    bool LooksLikeProjectRoot(const std::filesystem::path& dir)
    {
        std::error_code ec;
        if (std::filesystem::exists(dir / "Game.vcxproj", ec)) {
            return true;
        }

        return std::filesystem::exists(dir / "Data", ec)
            && std::filesystem::exists(dir / "Source", ec);
    }

    std::filesystem::path FindProjectRootFrom(std::filesystem::path start)
    {
        if (start.empty()) {
            return {};
        }

        std::error_code ec;
        start = std::filesystem::absolute(start, ec);
        if (ec) {
            return {};
        }

        if (!std::filesystem::is_directory(start, ec)) {
            start = start.parent_path();
        }

        for (std::filesystem::path dir = start.lexically_normal(); !dir.empty(); dir = dir.parent_path()) {
            if (LooksLikeProjectRoot(dir)) {
                return dir;
            }

            const std::filesystem::path parent = dir.parent_path();
            if (parent == dir) {
                break;
            }
        }

        return {};
    }

    std::filesystem::path GetExecutableDirectory()
    {
        std::vector<char> buffer(MAX_PATH);

        for (;;) {
            const DWORD length = GetModuleFileNameA(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
            if (length == 0) {
                return {};
            }

            if (length < buffer.size() - 1) {
                return std::filesystem::path(buffer.data()).parent_path();
            }

            buffer.resize(buffer.size() * 2);
        }
    }

    std::string NormalizeRootString(const std::filesystem::path& root)
    {
        std::string result = root.lexically_normal().string();
        std::replace(result.begin(), result.end(), '/', '\\');
        if (!result.empty() && result.back() != '\\') {
            result += "\\";
        }
        return result;
    }
}

void PathResolver::Initialize()
{
    std::filesystem::path root = FindProjectRootFrom(GetExecutableDirectory());

    if (root.empty()) {
        std::error_code ec;
        root = FindProjectRootFrom(std::filesystem::current_path(ec));
    }

    if (root.empty()) {
        std::error_code ec;
        root = std::filesystem::current_path(ec);
    }

    if (!root.empty()) {
        s_RootPath = NormalizeRootString(root);
    }
}

std::string PathResolver::Resolve(const std::string& inputPath)
{
    if (inputPath.empty()) return "";
    if (s_RootPath.empty()) {
        Initialize();
    }

    std::string path = inputPath;
    std::replace(path.begin(), path.end(), '/', '\\');

    size_t pos = path.find("Data\\");
    const std::filesystem::path fsPath(path);
    if (fsPath.is_absolute() && pos == std::string::npos) {
        return fsPath.lexically_normal().string();
    }

    std::string relativePart;
    if (pos != std::string::npos)
    {
        relativePart = path.substr(pos);
    }
    else
    {
        relativePart = path;
    }

    return (std::filesystem::path(s_RootPath) / relativePart).lexically_normal().string();
}
