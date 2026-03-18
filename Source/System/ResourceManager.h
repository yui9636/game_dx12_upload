#pragma once
#include <string>
#include <unordered_map>
#include <memory>
#include <d3d11.h>
#include <wrl/client.h>
#include "Material/MaterialAsset.h"
#include "RHI/ITexture.h"

class Model;

// シングルトンによるリソース管理クラス
class ResourceManager
{
public:
    static ResourceManager& Instance() {
        static ResourceManager instance;
        return instance;
    }

    // キャッシュをクリア（シーン切り替え時などに呼ぶ）
    void Clear();

    // モデルを取得（キャッシュになければロード）
    // scalingはモデルデータ自体の倍率（既存のModelコンストラクタ仕様に準拠）
    std::shared_ptr<Model> GetModel(const std::string& path, float scaling = 1.0f);

    // テクスチャを取得（キャッシュになければロード）
    std::shared_ptr<ITexture> GetTexture(const std::string& path);

    std::shared_ptr<MaterialAsset> GetMaterial(const std::string& path);

    std::shared_ptr<MaterialAsset> GetDefaultMaterial();
private:
    ResourceManager() = default;
    ~ResourceManager() = default;

    // キーは Resolve 前の「オリジナルパス」。これにより同一アセットの重複ロードを完全に防ぐ
    std::unordered_map<std::string, std::shared_ptr<Model>> modelMap;
    std::unordered_map<std::string, std::shared_ptr<ITexture>> textureMap;

    std::unordered_map<std::string, std::shared_ptr<MaterialAsset>> m_materials;

    std::shared_ptr<MaterialAsset> m_defaultMaterial;
};