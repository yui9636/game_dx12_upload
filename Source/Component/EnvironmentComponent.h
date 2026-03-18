#pragma once
#include <string>

struct EnvironmentComponent {
    // Skybox‚Ìİ’è
    bool enableSkybox = true;
    std::string skyboxPath = "Data/Texture/IBL/Skybox.dds";

    // IBL (Image Based Lighting) ‚Ìİ’è
    std::string diffuseIBLPath = "Data/Texture/IBL/diffuse_iem.dds";
    std::string specularIBLPath = "Data/Texture/IBL/specular_pmrem.dds";
};