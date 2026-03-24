#pragma once
#include "EffectNode.h"
#include <string>
#include <memory>
#include <wrl/client.h>
#include <d3d11.h>
#include "Model/Model.h"
#include "ShaderClass/EffectVariantShader.h"
#include "Effect/EffectMaterial.h"
#include "EffectCurve.h"

class MeshEmitter : public EffectNode
{
public:
    MeshEmitter();
    ~MeshEmitter() override;

    std::shared_ptr<::Model> model;
    std::shared_ptr<EffectMaterial> material;
    std::shared_ptr<EffectVariantShader> baseShader;

 
    std::string modelPath;
    std::string GetModelPath() const { return modelPath; }

    Microsoft::WRL::ComPtr<ID3D11PixelShader> pixelShaderVariant;

    EffectNodeType GetType() const override { return EffectNodeType::MeshEmitter; }

    void Update(float deltaTime) override;

    void Render(const struct RenderContext& rc);

    void LoadModel(const std::string& path);

    void RefreshPixelShader();

    void SetPixelShader(Microsoft::WRL::ComPtr<ID3D11PixelShader> ps) { pixelShaderVariant = ps; }

public:
 
    EffectCurve visibilityCurve;
    EffectCurve dissolveCurve;
    EffectCurve emissiveCurve;
    EffectCurve colorCurves[3];
    EffectCurve distortionStrengthCurve;
    EffectCurve wpoStrengthCurve;
    EffectCurve flowStrengthCurve;
    EffectCurve flowSpeedCurve;
    EffectCurve fresnelPowerCurve;
    EffectCurve mainUvScrollCurves[2];
    EffectCurve distUvScrollCurves[2];
    EffectCurve clipStartCurve;         
    EffectCurve clipEndCurve;
    EffectCurve clipSoftnessCurve;

    std::shared_ptr<::Model> GetModel() const { return model; }

    void UpdateWithAge(float age, float lifeTime) override;

    bool  ghostEnabled = false;
    int   ghostCount = 3;
    float ghostTimeDelay = 0.05f;
    float ghostAlphaDecay = 0.5f;
    DirectX::XMFLOAT3 ghostPosOffset = { 0.0f, 0.0f, 0.0f };

    float m_currentAge = 0.0f;

private:
  
    void ApplyCurves(float t);


};
