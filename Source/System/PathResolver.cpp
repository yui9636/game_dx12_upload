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
        // 区切り文字を統一し、末尾に必ずスラッシュを入れる
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
    // バックスラッシュに統一
    std::replace(path.begin(), path.end(), '/', '\\');

    // 1. パスの中から "Data\\" を探す
    // これにより "C:\Users\Name\...\Data\..." から相対部分だけを抽出する
    size_t pos = path.find("Data\\");

    std::string relativePart;
    if (pos != std::string::npos)
    {
        // "Data\\" 以降を切り出す
        relativePart = path.substr(pos);
    }
    else
    {
        // "Data" が見つからない場合は既に相対パスとみなす
        relativePart = path;
    }

    // 2. 確定した起動時ルートパスと結合して、このPCでのフルパスを完成させる
    return s_RootPath + relativePart;
}