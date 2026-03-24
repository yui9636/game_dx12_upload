#include "MeshEmitter.h"
#include "Graphics.h"
#include "EffectManager.h"
#include "RenderContext/RenderContext.h"
#include "ShaderClass/ShaderCompiler.h"
#include <iostream>
#include "RHI/ICommandList.h"

MeshEmitter::MeshEmitter() : EffectNode()
{
    material = std::make_shared<EffectMaterial>();

    pixelShaderVariant = EffectManager::Get().GetPixelShaderVariant(0);
}

MeshEmitter::~MeshEmitter() {}

void MeshEmitter::LoadModel(const std::string& path)
{
 /*   this->modelPath = path;
    auto device = Graphics::Instance().GetDevice();
    model = std::make_shared<Model>(device, path.c_str(), 1.0f);*/

    this->modelPath = path;
    this->model = EffectManager::Get().GetModel(path);

}

void MeshEmitter::Update(float deltaTime)
{
    EffectNode::Update(deltaTime);
}

void MeshEmitter::UpdateWithAge(float age, float lifeTime)
{
    EffectNode::UpdateWithAge(age, lifeTime);

    if (!material) return;

    m_currentAge = age;

    float nodeLocalTime = age - startTime;
    float t = 0.0f;

    if (duration > 0.001f) {
        t = std::clamp(nodeLocalTime / duration, 0.0f, 1.0f);
    }
    ApplyCurves(t);
}

void MeshEmitter::ApplyCurves(float t)
{
    if (!material) return;
    auto& c = material->GetConstants();

    if (visibilityCurve.IsValid())         c.visibility = visibilityCurve.Evaluate(t);
    if (dissolveCurve.IsValid())           c.dissolveThreshold = dissolveCurve.Evaluate(t);
    if (emissiveCurve.IsValid())           c.emissiveIntensity = emissiveCurve.Evaluate(t);

    if (colorCurves[0].IsValid()) c.baseColor.x = colorCurves[0].Evaluate(t);
    if (colorCurves[1].IsValid()) c.baseColor.y = colorCurves[1].Evaluate(t);
    if (colorCurves[2].IsValid()) c.baseColor.z = colorCurves[2].Evaluate(t);

    if (distortionStrengthCurve.IsValid()) c.distortionStrength = distortionStrengthCurve.Evaluate(t);
    if (wpoStrengthCurve.IsValid())        c.wpoStrength = wpoStrengthCurve.Evaluate(t);
    if (flowStrengthCurve.IsValid())       c.flowStrength = flowStrengthCurve.Evaluate(t);
    if (flowSpeedCurve.IsValid())          c.flowSpeed = flowSpeedCurve.Evaluate(t);
    if (fresnelPowerCurve.IsValid())       c.fresnelPower = fresnelPowerCurve.Evaluate(t);

    if (mainUvScrollCurves[0].IsValid())   c.mainUvScrollSpeed.x = mainUvScrollCurves[0].Evaluate(t);
    if (mainUvScrollCurves[1].IsValid())   c.mainUvScrollSpeed.y = mainUvScrollCurves[1].Evaluate(t);

    if (distUvScrollCurves[0].IsValid())   c.distortionUvScrollSpeed.x = distUvScrollCurves[0].Evaluate(t);
    if (distUvScrollCurves[1].IsValid())   c.distortionUvScrollSpeed.y = distUvScrollCurves[1].Evaluate(t);

    if (clipStartCurve.IsValid())          c.clipStart = clipStartCurve.Evaluate(t);
    if (clipEndCurve.IsValid())            c.clipEnd = clipEndCurve.Evaluate(t);

    if (clipSoftnessCurve.IsValid())       c.clipSoftness = clipSoftnessCurve.Evaluate(t);
}


//void MeshEmitter::Render(const RenderContext& rc)
//{
//    if (!model) return;
//
//    model->UpdateTransform(worldMatrix);
//
//    auto shaderToUse = EffectManager::Get().GetStandardShader();
//    if (shaderToUse)
//    {
//        shaderToUse->Begin(rc);
//
//        if (pixelShaderVariant)
//        {
//            rc.commandList->GetNativeContext()->PSSetShader(pixelShaderVariant.Get(), nullptr, 0);
//        }
//
//        if (material) {
//            material->Apply(rc);
//        }
//
//        shaderToUse->Draw(rc, model->GetModelResource().get());
//
//        shaderToUse->End(rc);
//    }
//}

void MeshEmitter::Render(const RenderContext& rc)
{
    if (!model) return;

    auto shaderToUse = EffectManager::Get().GetStandardShader();
    if (!shaderToUse) return;

    shaderToUse->Begin(rc);

    if (pixelShaderVariant)
    {
        rc.commandList->GetNativeContext()->PSSetShader(pixelShaderVariant.Get(), nullptr, 0);
    }

    auto& c = material->GetConstants();

    float originalTime = c.currentTime;
    float originalVisibility = c.visibility;
    DirectX::XMFLOAT4X4 originalWorld = worldMatrix;

    int maxLoop = ghostEnabled ? ghostCount : 0;

    for (int i = maxLoop; i >= 0; --i)
    {
        // -------------------------------------------------------------
        // -------------------------------------------------------------
        float ghostAge = m_currentAge - (i * ghostTimeDelay);

        // -------------------------------------------------------------
        // -------------------------------------------------------------
        if (ghostAge < 0.0f) continue;

        if (duration > 0.001f && ghostAge > duration) continue;

        // -------------------------------------------------------------
        // -------------------------------------------------------------
        float ghostLocalTime = ghostAge - startTime;
        float ghostT = 0.0f;

        if (duration > 0.001f) {
            ghostT = std::clamp(ghostLocalTime / duration, 0.0f, 1.0f);
        }

        ApplyCurves(ghostT);


        // -------------------------------------------------------------
        // -------------------------------------------------------------

        c.currentTime = originalTime - (i * ghostTimeDelay);

        float alphaRatio = std::pow(ghostAlphaDecay, (float)i);
        c.visibility *= alphaRatio;

        if (c.visibility < 0.01f) continue;

        if (i > 0)
        {
            DirectX::XMMATRIX m = DirectX::XMLoadFloat4x4(&originalWorld);
            m.r[3] = DirectX::XMVectorAdd(m.r[3],
                DirectX::XMVectorSet(
                    -ghostPosOffset.x * i,
                    -ghostPosOffset.y * i,
                    -ghostPosOffset.z * i,
                    0.0f
                )
            );
            DirectX::XMStoreFloat4x4(&worldMatrix, m);
        }
        else
        {
            worldMatrix = originalWorld;
        }

        if (material) {
            material->Apply(rc);
        }
        model->UpdateTransform(worldMatrix);
        shaderToUse->Draw(rc, model->GetModelResource().get());
    }

    shaderToUse->End(rc);

    // -------------------------------------------------------------
    // -------------------------------------------------------------
    worldMatrix = originalWorld;
    c.currentTime = originalTime;
    c.visibility = originalVisibility;

    float currentLocalTime = m_currentAge - startTime;
    float currentT = (duration > 0.001f) ? std::clamp(currentLocalTime / duration, 0.0f, 1.0f) : 0.0f;
    ApplyCurves(currentT);
}




void MeshEmitter::RefreshPixelShader()
{
    if (!material) return;

    auto& c = material->GetConstants();
    int flags = 0; // ShaderFlag_None

    if (c.mainTexIndex >= 0) {
        flags |= ShaderFlag_Texture;
    }

    if (c.distortionTexIndex >= 0) {
        flags |= ShaderFlag_Distort;
    }

    if (c.dissolveTexIndex >= 0) {
        flags |= ShaderFlag_Dissolve;

        if (c.dissolveGlowIntensity > 0.01f)
        {
            flags |= ShaderFlag_DissolveGlow;
        }
    }

    if (c.maskTexIndex >= 0) {
        flags |= ShaderFlag_Mask;
    }

    if (c.fresnelPower > 0.01f) {
        flags |= ShaderFlag_Fresnel;
    }

    if (c.flipbookWidth > 1.0f || c.flipbookHeight > 1.0f) {
        flags |= ShaderFlag_Flipbook;
    }

    if(c.gradientTexIndex >= 0) {
		flags |= ShaderFlag_GradientMap;
	}

    if (c.chromaticAberrationStrength > 0.001f) {
		flags |= ShaderFlag_ChromaticAberration;
	}

    if(c.matCapTexIndex >= 0) {
		flags |= ShaderFlag_MatCap;
	}

    if (c.normalTexIndex >= 0)
    {
        flags |= ShaderFlag_NormalMap;
    }

    if (c.flowTexIndex >= 0) 
    {
        flags |= ShaderFlag_FlowMap;
    }

    if (c.sideFadeWidth > 0.001f)
    {
        flags |= ShaderFlag_SideFade;
    }

    //{
    //    flags |= ShaderFlag_AlphaFade;
    //}

    flags |= ShaderFlag_AlphaFade;


    if (c.subTexIndex >= 0)
    {
        flags |= ShaderFlag_SubTexture;
    }

    if (c.toonRampTexIndex >=0)
    {
        flags |= ShaderFlag_Toon;
    }


    this->pixelShaderVariant = EffectManager::Get().GetPixelShaderVariant(flags);
}
