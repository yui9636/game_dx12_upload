#pragma once
#include <string>
#include <memory>

class MaterialAsset;

// ★ マテリアルアセットを「指し示す」だけの、極限まで無駄を削ぎ落とした姿
struct MaterialComponent {
    std::string materialAssetPath = "";
    std::shared_ptr<MaterialAsset> materialAsset;
};