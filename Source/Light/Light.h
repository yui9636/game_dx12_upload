#pragma once

#include <DirectXMath.h>
#include <vector>
#include"Sprite/Sprite.h"

struct PointLight
{
    DirectX::XMFLOAT3 position;
    float range;
    DirectX::XMFLOAT3 color;    
    float intensity;
};

struct DirectionalLight
{
    DirectX::XMFLOAT3 direction;
    DirectX::XMFLOAT3 color;
  
};

//class LightManager
//{
//public:
//    static LightManager& Instance()
//    {
//        static LightManager lightManager;
//        return lightManager;
//    }
//
//    void SetDirectionalLight(const DirectionalLight& light) { directionalLight = light; }
//    const DirectionalLight& GetDirectionalLight() const { return directionalLight; }
//
//
//    void ClearPointLights() { pointLights.clear(); }
//
//    void AddPointLight(const PointLight& light) { pointLights.push_back(light); }
//
//    const std::vector<PointLight>& GetPointLights() const { return pointLights; }
//
//private:
//    DirectionalLight directionalLight;
//
//    std::vector<PointLight> pointLights;
//};
