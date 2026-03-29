// AssetManager.h

#pragma once

#include <string>

#include <vector>

#include <memory>

#include <filesystem>

#include <imgui.h>

#include <d3d11.h>

#include <wrl/client.h>

#include "Icon/IconFontManager.h" //



class ITexture;



enum class AssetType { Folder, Model, Texture, Font, Prefab, Script, Audio, Material, Unknown };



struct AssetEntry {

    std::string fileName;

    std::filesystem::path path;

    AssetType type;




    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> thumbnail;

    std::shared_ptr<ITexture> thumbnailTexture;

    const char* iconStr = nullptr;

    ImVec4 iconColor = { 1, 1, 1, 1 };

};



class AssetManager {

public:

    static AssetManager& Instance() { static AssetManager instance; return instance; }



    void Initialize(const std::string& rootDirectory);

    std::vector<AssetEntry> GetAssetsInDirectory(const std::filesystem::path& directory);

    void OpenInExternalEditor(const std::filesystem::path& path);



    const std::filesystem::path& GetRootDirectory() const { return m_rootDirectory; }



    void ImportExternalFile(const std::filesystem::path& sourcePath, const std::filesystem::path& destinationDir);



    void CreateNewFolder(const std::filesystem::path& parentDir);

    void CreateNewScript(const std::filesystem::path& parentDir);

    void CreateNewShader(const std::filesystem::path& parentDir);

    void CreateNewMaterial(const std::filesystem::path& parentDir);

    bool RenameAsset(const std::filesystem::path& oldPath, const std::string& newName);

    bool CopyAsset(const std::filesystem::path& sourcePath, const std::filesystem::path& destinationDir);

    bool MoveAsset(const std::filesystem::path& sourcePath, const std::filesystem::path& destinationDir);

    void DeleteAsset(const std::filesystem::path& path);

private:

    AssetManager() = default;

    std::filesystem::path m_rootDirectory;

    void AssignIconAndType(AssetEntry& entry);

};

