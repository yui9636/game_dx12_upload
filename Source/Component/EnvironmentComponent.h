#pragma once
#include <string>

struct EnvironmentComponent {
    bool enableSkybox = true;
    std::string skyboxPath = "Data/Texture/IBL/Skybox.dds";

    std::string diffuseIBLPath = "Data/Texture/IBL/diffuse_iem.dds";
    std::string specularIBLPath = "Data/Texture/IBL/specular_pmrem.dds";
};
