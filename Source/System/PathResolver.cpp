#include "PathResolver.h"
#include <windows.h>
#include <direct.h>
#include <algorithm>

std::string PathResolver::s_RootPath = "";

void PathResolver::Initialize()
{
    char buffer[1024];
    if (_getcwd(buffer, 1024))
    {
        s_RootPath = buffer;
        std::replace(s_RootPath.begin(), s_RootPath.end(), '/', '\\');
        if (!s_RootPath.empty() && s_RootPath.back() != '\\')
        {
            s_RootPath += "\\";
        }
    }
}

std::string PathResolver::Resolve(const std::string& inputPath)
{
    if (inputPath.empty()) return "";

    std::string path = inputPath;
    std::replace(path.begin(), path.end(), '/', '\\');

    size_t pos = path.find("Data\\");

    std::string relativePart;
    if (pos != std::string::npos)
    {
        relativePart = path.substr(pos);
    }
    else
    {
        relativePart = path;
    }

    return s_RootPath + relativePart;
}
