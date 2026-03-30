// AssetManager.cpp

#include "AssetManager.h"

#include <windows.h>

#include <shellapi.h>

#include "ThumbnailGenerator.h"

#include "System/ResourceManager.h"

#include <fstream>

#include "Graphics.h"

#include "RHI/ITexture.h"

#include "RHI/DX11/DX11Texture.h"
#include "Console/Logger.h"



void AssetManager::Initialize(const std::string& rootDirectory) {

    m_rootDirectory = rootDirectory;

    if (!std::filesystem::exists(m_rootDirectory)) std::filesystem::create_directories(m_rootDirectory);



    std::filesystem::path parentDir = std::filesystem::path(m_rootDirectory).parent_path();



}



std::vector<AssetEntry> AssetManager::GetAssetsInDirectory(const std::filesystem::path& directory) {

    std::vector<AssetEntry> entries;

    std::error_code ec;



    if (!std::filesystem::exists(directory, ec) || !std::filesystem::is_directory(directory, ec)) return entries;



    // ==========================================



    // ==========================================

    auto it = std::filesystem::directory_iterator(directory, ec);

    if (ec) return entries;



    for (; it != std::filesystem::directory_iterator(); it.increment(ec)) {

        if (ec) {

            ec.clear();

            continue;

        }



        AssetEntry asset;

        asset.path = it->path();

        asset.fileName = it->path().filename().string();

        AssignIconAndType(asset);

        entries.push_back(asset);

    }




    std::sort(entries.begin(), entries.end(), [](const AssetEntry& a, const AssetEntry& b) {

        if (a.type == AssetType::Folder && b.type != AssetType::Folder) return true;

        if (a.type != AssetType::Folder && b.type == AssetType::Folder) return false;

        return a.fileName < b.fileName;

        });



    return entries;

}









void AssetManager::AssignIconAndType(AssetEntry& entry) {

    if (std::filesystem::is_directory(entry.path)) {

        entry.type = AssetType::Folder;

        entry.iconStr = ICON_FA_FOLDER;

        entry.iconColor = ImVec4(0.9f, 0.7f, 0.2f, 1.0f);

        return;

    }



    std::string ext = entry.path.extension().string();

    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);



    // ==========================================


    // ==========================================

    if (ext == ".fbx" || ext == ".obj" || ext == ".blend" || ext == ".gltf"|| ext == ".cereal") {

        entry.type = AssetType::Model;

        entry.iconStr = ICON_FA_CUBE;

        entry.iconColor = ImVec4(0.4f, 0.8f, 0.9f, 1.0f);

        ThumbnailGenerator::Instance().Request(entry.path.string());
        entry.thumbnailTexture = ThumbnailGenerator::Instance().Get(entry.path.string());

    }

    // ==========================================


    // ==========================================

    else if (ext == ".prefab" || ext == ".bin" || ext == ".json") {


        entry.type = AssetType::Prefab;

        entry.iconStr = ICON_FA_BOXES_STACKED;

        entry.iconColor = ImVec4(0.6f, 0.4f, 0.9f, 1.0f);





        entry.thumbnail = nullptr;

    }

    // ==========================================


    // ==========================================

    else if (ext == ".png" || ext == ".jpg" || ext == ".tga" || ext == ".dds" || ext == ".hdr") {

        entry.type = AssetType::Texture;

        entry.iconStr = ICON_FA_IMAGE;

        entry.iconColor = ImVec4(0.5f, 0.9f, 0.5f, 1.0f);



        auto tex = ResourceManager::Instance().GetTexture(entry.path.string());

        entry.thumbnailTexture = tex;

        if (Graphics::Instance().GetAPI() == GraphicsAPI::DX11 && tex) {

            auto* dx11Texture = dynamic_cast<DX11Texture*>(tex.get());

            entry.thumbnail = dx11Texture ? dx11Texture->GetNativeSRV() : nullptr;

        }



    }

    else if (ext == ".ttf" || ext == ".otf" || ext == ".fnt") {

        entry.type = AssetType::Font;

        entry.iconStr = ICON_FA_FONT;

        entry.iconColor = ImVec4(0.95f, 0.85f, 0.45f, 1.0f);

    }

    else if (ext == ".wav" || ext == ".ogg" || ext == ".mp3" || ext == ".flac") {

        entry.type = AssetType::Audio;

        entry.iconStr = ICON_FA_VOLUME_HIGH;

        entry.iconColor = ImVec4(0.45f, 0.85f, 1.0f, 1.0f);

    }

    else if (ext == ".mat") {

        entry.type = AssetType::Material;

        entry.iconStr = ICON_FA_PALETTE;

        entry.iconColor = ImVec4(1.0f, 0.5f, 0.8f, 1.0f);

        ThumbnailGenerator::Instance().RequestMaterial(entry.path.string());
        entry.thumbnailTexture = ThumbnailGenerator::Instance().Get(entry.path.string());

    }

    else if (ext == ".cpp" || ext == ".h" || ext == ".hlsl" || ext == ".hlsli") {

        entry.type = AssetType::Script;

        entry.iconStr = ICON_FA_FILE_CODE;

        entry.iconColor = (ext == ".hlsl" || ext == ".hlsli")

            ? ImVec4(0.3f, 0.9f, 0.6f, 1.0f)

            : ImVec4(0.8f, 0.8f, 0.8f, 1.0f);

    }

    else {

        entry.type = AssetType::Unknown;

        entry.iconStr = ICON_FA_FILE;

        entry.iconColor = ImVec4(0.5f, 0.5f, 0.5f, 1.0f);

    }

}



void AssetManager::OpenInExternalEditor(const std::filesystem::path& path) {

    ShellExecuteA(NULL, "open", path.string().c_str(), NULL, NULL, SW_SHOWNORMAL);

}



void AssetManager::ImportExternalFile(const std::filesystem::path& sourcePath, const std::filesystem::path& destinationDir) {

    std::error_code ec;




    std::filesystem::path finalDest = destinationDir / sourcePath.filename();




    int count = 1;

    while (std::filesystem::exists(finalDest, ec)) {

        std::string newName = sourcePath.stem().string() + "(" + std::to_string(count++) + ")" + sourcePath.extension().string();

        finalDest = destinationDir / newName;

    }




    if (std::filesystem::copy_file(sourcePath, finalDest, std::filesystem::copy_options::none, ec)) {


    }

}



void AssetManager::CreateNewFolder(const std::filesystem::path& parentDir) {

    std::error_code ec;

    std::filesystem::path newPath = parentDir / "NewFolder";

    int count = 1;

    while (std::filesystem::exists(newPath, ec)) {

        newPath = parentDir / ("NewFolder (" + std::to_string(count++) + ")");

    }

    std::filesystem::create_directory(newPath, ec);

}



void AssetManager::CreateNewScript(const std::filesystem::path& parentDir) {

    std::error_code ec;




    std::string baseName = "NewActor";

    std::filesystem::path hPath = parentDir / (baseName + ".h");

    std::filesystem::path cppPath = parentDir / (baseName + ".cpp");




    int count = 1;

    while (std::filesystem::exists(hPath, ec) || std::filesystem::exists(cppPath, ec)) {

        baseName = "NewActor_" + std::to_string(count++);

        hPath = parentDir / (baseName + ".h");

        cppPath = parentDir / (baseName + ".cpp");

    }




    std::ofstream ofsH(hPath);

    ofsH << "#pragma once\n\n"

        << "#include \"Actor/Actor.h\"\n\n"

        << "class " << baseName << " : public Actor {\n"

        << "public:\n"

        << "    void Update(float dt) override;\n"

        << "};\n";

    ofsH.close();




    std::ofstream ofsCpp(cppPath);

    ofsCpp << "#include \"" << baseName << ".h\"\n\n"

        << "void " << baseName << "::Update(float dt) {\n"

        << "      // ŽÀ‘•‚ð‚±‚±‚É’Ç‰Á‚·‚é\\n"

        << "}\\n";

    ofsCpp.close();

}



void AssetManager::CreateNewShader(const std::filesystem::path& parentDir) {

    std::error_code ec;

    std::filesystem::path newPath = parentDir / "NewShader.hlsl";

    int count = 1;

    while (std::filesystem::exists(newPath, ec)) {

        newPath = parentDir / ("NewShader (" + std::to_string(count++) + ").hlsl");

    }

    std::ofstream ofs(newPath);

    ofs << "float4 VSMain(float4 pos : POSITION) : SV_POSITION {\n    return pos;\n}\n\nfloat4 PSMain() : SV_TARGET {\n    return float4(1, 1, 1, 1);\n}\n";

    ofs.close();

}



void AssetManager::CreateNewMaterial(const std::filesystem::path& parentDir) {

    std::error_code ec;

    std::filesystem::path newPath = parentDir / "NewMaterial.mat";

    int count = 1;

    while (std::filesystem::exists(newPath, ec)) {

        newPath = parentDir / ("NewMaterial (" + std::to_string(count++) + ").mat");

    }




    std::ofstream ofs(newPath);

    if (ofs.is_open()) {

        ofs << "{\n"

            << "  \"baseColor\": [1.0, 1.0, 1.0, 1.0],\n"

            << "  \"metallic\": 0.0,\n"

            << "  \"roughness\": 0.5,\n"

            << "  \"emissive\": 0.0,\n"

            << "  \"diffuseTexturePath\": \"\",\n"

            << "  \"normalTexturePath\": \"\",\n"

            << "  \"metallicRoughnessTexturePath\": \"\",\n"

            << "  \"emissiveTexturePath\": \"\",\n"

            << "  \"shaderId\": 1,\n"

            << "  \"alphaMode\": 0\n"

            << "}";

        ofs.close();

    }

}



bool AssetManager::RenameAsset(const std::filesystem::path& oldPath, const std::string& newName) {

    std::error_code ec;

    std::filesystem::path newPath = oldPath.parent_path() / newName;

    if (std::filesystem::exists(newPath)) return false;



    std::filesystem::rename(oldPath, newPath, ec);

    return !ec;

}



void AssetManager::DeleteAsset(const std::filesystem::path& path) {

    std::error_code ec;

    std::filesystem::remove_all(path, ec);

}



bool AssetManager::MoveAsset(const std::filesystem::path& sourcePath, const std::filesystem::path& destinationDir) {

    std::error_code ec;

    std::filesystem::path finalDest = destinationDir / sourcePath.filename();




    if (std::filesystem::exists(finalDest)) {

        return false;

    }



    std::filesystem::rename(sourcePath, finalDest, ec);

    return !ec;

}



bool AssetManager::CopyAsset(const std::filesystem::path& sourcePath, const std::filesystem::path& destinationDir) {

    std::error_code ec;

    std::filesystem::path finalDest = destinationDir / sourcePath.filename();




    int count = 1;

    while (std::filesystem::exists(finalDest, ec)) {

        std::string newName = sourcePath.stem().string() + " - Copy (" + std::to_string(count++) + ")" + sourcePath.extension().string();

        finalDest = destinationDir / newName;

    }




    std::filesystem::copy(sourcePath, finalDest, std::filesystem::copy_options::recursive, ec);

    return !ec;

}



