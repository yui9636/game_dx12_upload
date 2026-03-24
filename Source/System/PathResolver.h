#pragma once
#include <string>


class PathResolver
{
public:
    static void Initialize();

    static std::string Resolve(const std::string& inputPath);

    static const std::string& GetRootPath() { return s_RootPath; }

private:
    static std::string s_RootPath;
};
