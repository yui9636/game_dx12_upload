#pragma once
#include <string>


class PathResolver
{
public:
    // アプリ起動時に一度だけ呼び出し、基準となるルートパスを確定させる
    static void Initialize();

    // どんなパス（絶対パス・相対パス・他人のパス）が来ても
    // 現在の実行環境における「正しいフルパス」に変換して返す
    static std::string Resolve(const std::string& inputPath);

    // プロジェクトのルートパスを取得する
    static const std::string& GetRootPath() { return s_RootPath; }

private:
    static std::string s_RootPath;
};