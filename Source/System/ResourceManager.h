#pragma once
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <memory>
#include <vector>
#include <d3d11.h>
#include <wrl/client.h>
#include "Material/MaterialAsset.h"
#include "RHI/ITexture.h"
// Model はこの機能の公開インターフェースを定義し、実装側が具体的な処理を行う。

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

    std::vector<std::string> ListLoadedModelKeys() const;
    std::vector<std::string> ListLoadedTextureKeys() const;
    std::vector<std::string> ListLoadedMaterialKeys() const;
private:
    ResourceManager() = default;
    ~ResourceManager() = default;

    std::unordered_map<std::string, std::shared_ptr<Model>> modelMap;
    std::unordered_map<std::string, std::shared_ptr<ITexture>> textureMap;
    std::unordered_set<std::string> m_failedTexturePaths;

    std::unordered_map<std::string, std::shared_ptr<MaterialAsset>> m_materials;

    std::shared_ptr<MaterialAsset> m_defaultMaterial;
};
