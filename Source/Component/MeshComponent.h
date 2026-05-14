#pragma once
#include <string>
#include <memory>

class Model;

/**
 * @brief 描画リソースと表示フラグを保持するコンポーネント。
 */
struct MeshComponent {
    std::shared_ptr<Model> model;

    std::string modelFilePath;

    bool isVisible = true;
    bool castShadow = true;
    bool isDebugModel = false;

 
};
