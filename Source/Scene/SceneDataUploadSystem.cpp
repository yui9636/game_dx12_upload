#include "SceneDataUploadSystem.h"
#include "Graphics.h"
#include "System/ResourceManager.h"
#include <algorithm>
#include "RHI/IBuffer.h"
#include "RHI/ICommandList.h"
#include "Render/GlobalRootSignature.h"
#include "RHI/DX12/DX12Texture.h"
#include "Console/Logger.h"

void SceneDataUploadSystem::Upload(const RenderContext& rc, GlobalRootSignature& rootSig)
{
    CbScene scene{};
    DirectX::XMMATRIX V = DirectX::XMLoadFloat4x4(&rc.viewMatrix);
    DirectX::XMMATRIX P = DirectX::XMLoadFloat4x4(&rc.projectionMatrix);
    DirectX::XMStoreFloat4x4(&scene.viewProjection, V * P);

    scene.viewProjectionUnjittered = rc.viewProjectionUnjittered;
    scene.prevViewProjection = rc.prevViewProjectionMatrix;
    scene.lightDirection = { rc.directionalLight.direction.x, rc.directionalLight.direction.y, rc.directionalLight.direction.z, 0.0f };
    scene.lightColor = { rc.directionalLight.color.x, rc.directionalLight.color.y, rc.directionalLight.color.z, 1.0f };
    scene.cameraPosition = { rc.cameraPosition.x, rc.cameraPosition.y, rc.cameraPosition.z, 1.0f };

    scene.jitterX = rc.jitterOffset.x;
    scene.jitterY = rc.jitterOffset.y;
    scene.prevJitterX = rc.prevJitterOffset.x;
    scene.prevJitterY = rc.prevJitterOffset.y;

    float renderScale = Graphics::Instance().GetRenderScale();
    scene.renderW = (float)(uint32_t)(Graphics::Instance().GetScreenWidth() * renderScale);
    scene.renderH = (float)(uint32_t)(Graphics::Instance().GetScreenHeight() * renderScale);
    scene.shadowTexelSize = 1.0f / 2048.0f;
    scene.shadowColor = { rc.shadowColor.x, rc.shadowColor.y, rc.shadowColor.z, 1.0f };

    int count = static_cast<int>(rc.pointLights.size());
    if (count > MAX_POINT_LIGHTS) count = MAX_POINT_LIGHTS;

    scene.pointLightCount = static_cast<float>(count);
    for (int i = 0; i < count; ++i) {
        scene.pointLights[i].position = rc.pointLights[i].position;
        scene.pointLights[i].range = rc.pointLights[i].range;
        scene.pointLights[i].color = rc.pointLights[i].color;
        scene.pointLights[i].intensity = rc.pointLights[i].intensity;
    }

    rc.commandList->UpdateBuffer(rootSig.GetSceneBuffer(), &scene, sizeof(scene));

    float shadowSplit0 = 0.0f;
    float shadowSplit1 = 0.0f;
    float shadowSplit2 = 0.0f;
    if (rc.shadowMap) {
        CbShadowMap shadow{};
        for (int i = 0; i < 3; ++i) {
            shadow.lightViewProjections[i] = rc.shadowMap->GetLightViewProjection(i);
        }
        shadow.shadowColor = { rc.shadowColor.x, rc.shadowColor.y, rc.shadowColor.z, 1.0f };
        shadow.shadowBias = { 0.005f, 0.0f, 0.0f, 0.0f };

        shadow.cascadeSplits.x = rc.shadowMap->GetCascadeEnd(0);
        shadow.cascadeSplits.y = rc.shadowMap->GetCascadeEnd(1);
        shadow.cascadeSplits.z = rc.shadowMap->GetCascadeEnd(2);
        shadowSplit0 = shadow.cascadeSplits.x;
        shadowSplit1 = shadow.cascadeSplits.y;
        shadowSplit2 = shadow.cascadeSplits.z;

        rc.commandList->UpdateBuffer(rootSig.GetShadowBuffer(), &shadow, sizeof(shadow));
    }

    auto diffTex = rc.environment.diffuseIBLPath.empty() ? nullptr : ResourceManager::Instance().GetTexture(rc.environment.diffuseIBLPath);
    auto specTex = rc.environment.specularIBLPath.empty() ? nullptr : ResourceManager::Instance().GetTexture(rc.environment.specularIBLPath);

    static bool s_loggedSceneInputs = false;
    if (!s_loggedSceneInputs) {
        LOG_INFO("[SceneDataUpload] dirLight=(%.3f, %.3f, %.3f) color=(%.3f, %.3f, %.3f) pointCount=%d shadowSplits=(%.3f, %.3f, %.3f)",
            scene.lightDirection.x, scene.lightDirection.y, scene.lightDirection.z,
            scene.lightColor.x, scene.lightColor.y, scene.lightColor.z,
            count,
            shadowSplit0, shadowSplit1, shadowSplit2);

        auto logCubemap = [](const char* label, const std::shared_ptr<ITexture>& tex) {
            if (!tex) {
                LOG_INFO("[SceneDataUpload] %s=null", label);
                return;
            }
            if (auto* dx12 = dynamic_cast<DX12Texture*>(tex.get())) {
                auto desc = dx12->GetNativeResource()->GetDesc();
                LOG_INFO("[SceneDataUpload] %s dx12 format=%d array=%u mip=%u hasSRV=%d",
                    label,
                    static_cast<int>(desc.Format),
                    static_cast<unsigned>(desc.DepthOrArraySize),
                    static_cast<unsigned>(desc.MipLevels),
                    dx12->HasSRV() ? 1 : 0);
            } else {
                LOG_INFO("[SceneDataUpload] %s tex=%p", label, tex.get());
            }
        };

        logCubemap("diffuseIBL", diffTex);
        logCubemap("specularIBL", specTex);
        s_loggedSceneInputs = true;
    }

    rootSig.SetIBL(diffTex.get(), specTex.get());
}
