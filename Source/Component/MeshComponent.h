#pragma once
#include <string>
#include <memory>

class Model;

/**
 * @brief ・ｽ`・ｽ諠奇ｿｽ\・ｽ[・ｽX・ｽﾆ表・ｽ・ｽ・ｽt・ｽ・ｽ・ｽO・ｽ・ｽﾇ暦ｿｽ・ｽ・ｽ・ｽ・ｽR・ｽ・ｽ・ｽ|・ｽ[・ｽl・ｽ・ｽ・ｽg
 */
struct MeshComponent {
    std::shared_ptr<Model> model;

    std::string modelFilePath;

    bool isVisible = true;
    bool castShadow = true;
    bool isDebugModel = false;

 
};
