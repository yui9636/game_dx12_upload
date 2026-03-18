#include "JSONManager.h"
#include <fstream>
#include <iostream>
#include <filesystem>
#include "System/PathResolver.h"
JSONManager::JSONManager(const std::string& filePath) : filePath(filePath) {
    Load();
}

void JSONManager::Load() {
    std::ifstream file(filePath);
    if (file.is_open()) {
        try { file >> jsonData; }
        catch (const std::exception& e) {
            std::cerr << "JSON load error: " << e.what() << std::endl;
        }
    }
}

void JSONManager::Save() const {
    std::ofstream file(filePath);
    if (file.is_open()) {
        try { file << jsonData.dump(4); }
        catch (const std::exception& e) {
            std::cerr << "JSON save error: " << e.what() << std::endl;
        }
    }
}



std::string JSONManager::ToRelativePath(const std::string& fullPath)
{
    if (fullPath.empty()) return "";

    try {
        std::filesystem::path p(fullPath);

        // すでに相対パス（"Data/"で始まる等）ならそのまま返す
        if (p.is_relative()) {
            return p.generic_string();
        }

        // 絶対パスの場合、現在の作業ディレクトリ(g_RootPath)からの相対パスを計算する
        // ※std::filesystem::relative はC++17の標準機能です
        std::filesystem::path relativePath = std::filesystem::relative(p, std::filesystem::current_path());

        return relativePath.generic_string(); // スラッシュを / に統一して返す
    }
    catch (...) {
        // エラー（ドライブを跨ぐ場合など）は元のパスを返す
        return fullPath;
    }
}