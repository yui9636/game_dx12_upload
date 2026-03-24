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

        if (p.is_relative()) {
            return p.generic_string();
        }

        std::filesystem::path relativePath = std::filesystem::relative(p, std::filesystem::current_path());

        return relativePath.generic_string();
    }
    catch (...) {
        return fullPath;
    }
}
