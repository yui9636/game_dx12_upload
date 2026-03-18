#pragma once

#include <DirectXMath.h>
#include <vector>
#include"Sprite/Sprite.h"

// ★追加: ポイントライト構造体
struct PointLight
{
    DirectX::XMFLOAT3 position;
    float range;
    DirectX::XMFLOAT3 color;    
    float intensity;
};

// ディレクショナルライト
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
//    // --- ディレクショナルライト ---
//    void SetDirectionalLight(const DirectionalLight& light) { directionalLight = light; }
//    const DirectionalLight& GetDirectionalLight() const { return directionalLight; }
//
//    // --- ★追加: ポイントライト管理 ---
//
//    // 毎フレームリセット用
//    void ClearPointLights() { pointLights.clear(); }
//
//    // ライト登録用
//    void AddPointLight(const PointLight& light) { pointLights.push_back(light); }
//
//    // 取得用
//    const std::vector<PointLight>& GetPointLights() const { return pointLights; }
//
//private:
//    DirectionalLight directionalLight;
//
//    // ★追加: ポイントライトリスト
//    std::vector<PointLight> pointLights;
//};