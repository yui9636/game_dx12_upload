#pragma once
#include <string>
// PathResolver は s_RootPath を中心に、実行時やエディターで共有する状態を保持する。


class PathResolver
{
public:
    static void Initialize();

    static std::string Resolve(const std::string& inputPath);

    static const std::string& GetRootPath() { return s_RootPath; }

private:
    static std::string s_RootPath;
};
