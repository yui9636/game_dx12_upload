#pragma once
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <memory>
#include <d3d11.h>
#include <wrl/client.h>
#include "Material/MaterialAsset.h"
#include "RHI/ITexture.h"

class Model;

class ResourceManager
{
public:
    static ResourceManager& Instance() {
        static ResourceManager instance;
        return instance;
    }

    void Clear();

    std::shared_ptr<Model> GetModel(const std::string& path, float scaling = 1.0f, bool sourceOnly = false);
    std::shared_ptr<Model> CreateModelInstance(const std::string& path, float scaling = 1.0f, bool sourceOnly = false);
    void InvalidateModel(const std::string& path);

    std::shared_ptr<ITexture> GetTexture(const std::string& path);

    std::shared_ptr<MaterialAsset> GetMaterial(const std::string& path);

    std::shared_ptr<MaterialAsset> GetDefaultMaterial();
private:
    ResourceManager() = default;
    ~ResourceManager() = default;

    std::unordered_map<std::string, std::shared_ptr<Model>> modelMap;
    std::unordered_map<std::string, std::shared_ptr<ITexture>> textureMap;
    std::unordered_set<std::string> m_failedTexturePaths;

    std::unordered_map<std::string, std::shared_ptr<MaterialAsset>> m_materials;

    std::shared_ptr<MaterialAsset> m_defaultMaterial;
};
