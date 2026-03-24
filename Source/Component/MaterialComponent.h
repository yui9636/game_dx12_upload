#pragma once
#include <string>
#include <memory>

class MaterialAsset;

struct MaterialComponent {
    std::string materialAssetPath = "";
    std::shared_ptr<MaterialAsset> materialAsset;
};
