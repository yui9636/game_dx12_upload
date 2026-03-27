#include "SceneDataUploadSystem.h"
#include "Graphics.h"
#include "System/ResourceManager.h"
#include <algorithm>
#include "RHI/IBuffer.h"
#include "RHI/ICommandList.h"
#include "Render/GlobalRootSignature.h"

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

    // DX12 は Phase 4 の途中段階で temporal 履歴契約をまだ完全に分離できていない。
    // まず main view の見た目破綻を止めるため、SSGI などが使う previous view は
    // 現在の unjittered 行列へフォールバックさせる。
    if (Graphics::Instance().GetAPI() == GraphicsAPI::DX12) {
        scene.prevViewProjection = scene.viewProjectionUnjittered;
        scene.prevJitterX = 0.0f;
        scene.prevJitterY = 0.0f;
    }

    const ITexture* sceneTarget = rc.sceneColorTexture ? rc.sceneColorTexture : rc.mainRenderTarget;
    const float renderWidth = rc.renderWidth > 0
        ? static_cast<float>(rc.renderWidth)
        : (sceneTarget
            ? static_cast<float>(sceneTarget->GetWidth())
            : (rc.mainViewport.width > 0.0f
                ? rc.mainViewport.width
                : static_cast<float>(Graphics::Instance().GetScreenWidth())));
    const float renderHeight = rc.renderHeight > 0
        ? static_cast<float>(rc.renderHeight)
        : (sceneTarget
            ? static_cast<float>(sceneTarget->GetHeight())
            : (rc.mainViewport.height > 0.0f
                ? rc.mainViewport.height
                : static_cast<float>(Graphics::Instance().GetScreenHeight())));
    scene.renderW = renderWidth;
    scene.renderH = renderHeight;
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

    IBuffer* sceneBuffer = rc.sceneConstantBufferOverride ? rc.sceneConstantBufferOverride : rootSig.GetSceneBuffer();
    rc.commandList->UpdateBuffer(sceneBuffer, &scene, sizeof(scene));

    float shadowSplit0 = 0.0f;
    float shadowSplit1 = 0.0f;
    float shadowSplit2 = 0.0f;
    if (rc.shadowMap) {
        CbShadowMap shadow{};
        for (int i = 0; i < 3; ++i) {
            shadow.lightViewProjections[i] = rc.shadowMap->GetLightViewProjection(i);
        }
        shadow.shadowColor = { rc.shadowColor.x, rc.shadowColor.y, rc.shadowColor.z, 1.0f };
        shadow.shadowBias = { 0.00002f, 0.0f, 0.0f, 0.0f };

        shadow.cascadeSplits.x = rc.shadowMap->GetCascadeEnd(0);
        shadow.cascadeSplits.y = rc.shadowMap->GetCascadeEnd(1);
        shadow.cascadeSplits.z = rc.shadowMap->GetCascadeEnd(2);
        shadowSplit0 = shadow.cascadeSplits.x;
        shadowSplit1 = shadow.cascadeSplits.y;
        shadowSplit2 = shadow.cascadeSplits.z;

        IBuffer* shadowBuffer = rc.shadowConstantBufferOverride ? rc.shadowConstantBufferOverride : rootSig.GetShadowBuffer();
        rc.commandList->UpdateBuffer(shadowBuffer, &shadow, sizeof(shadow));
    }

    auto diffTex = rc.environment.diffuseIBLPath.empty() ? nullptr : ResourceManager::Instance().GetTexture(rc.environment.diffuseIBLPath);
    auto specTex = rc.environment.specularIBLPath.empty() ? nullptr : ResourceManager::Instance().GetTexture(rc.environment.specularIBLPath);

    rootSig.SetIBL(diffTex.get(), specTex.get());
}
