#include "GameLayer.h"
#include "Graphics.h"
#include <Transform\TransformSystem.h>
#include <Mesh\MeshExtractSystem.h>
#include "Component/CameraComponent.h"
#include "Component/TransformComponent.h"
#include "Component/NameComponent.h"
#include "Component/MeshComponent.h"
#include "System/Query.h"
#include "System/ResourceManager.h"
#include <Camera\FreeCameraSystem.h>
#include <Camera\CameraFinalizeSystem.h>
#include "Model/ModelUpdateSystem.h"
#include <Component\CameraBehaviorComponent.h>
#include <Component\LightComponent.h>
#include "Component/EnvironmentComponent.h"
#include "Environment/EnvironmentExtractSystem.h"
#include <Component\ReflectionProbeComponent.h>
#include "RHI/DX11/DX11Texture.h"

void GameLayer::Initialize()
{
    // エディタ起動直後でも最低限の描画情報が成立するように、
    // デフォルトのカメラ・ライト・反射プローブを作っておく。
    EntityID cameraEntity = m_registry.CreateEntity();
    m_registry.AddComponent(cameraEntity, NameComponent{ "Main Camera" });

    TransformComponent camTrans;
    camTrans.localPosition = { 0.0f, 2.0f, -10.0f }; // モデルが見えるように少し上＆後ろに下がる
    m_registry.AddComponent(cameraEntity, camTrans);

    m_registry.AddComponent(cameraEntity, HierarchyComponent{});
    m_registry.AddComponent(cameraEntity, CameraFreeControlComponent{});

    m_registry.AddComponent(cameraEntity, CameraLensComponent{});
    m_registry.AddComponent(cameraEntity, CameraMatricesComponent{});
    m_registry.AddComponent(cameraEntity, CameraMainTagComponent{}); 


    EntityID lightEntity = m_registry.CreateEntity();
    m_registry.AddComponent(lightEntity, NameComponent{ "Directional Light" });

    TransformComponent lightTrans;
    // 斜め下を向くように回転（Pitch=45度, Yaw=45度）
    DirectX::XMVECTOR rot = DirectX::XMQuaternionRotationRollPitchYaw(
        DirectX::XMConvertToRadians(45.0f), DirectX::XMConvertToRadians(45.0f), 0.0f);
    DirectX::XMStoreFloat4(&lightTrans.localRotation, rot);

    m_registry.AddComponent(lightEntity, lightTrans);
    m_registry.AddComponent(lightEntity, HierarchyComponent{});

    LightComponent lightComp;
    lightComp.type = LightType::Directional;
    lightComp.color = { 1.0f, 1.0f, 1.0f }; // 白い光
    lightComp.intensity = 1.0f;             // 光の強さ
    m_registry.AddComponent(lightEntity, lightComp);

    EntityID probeEntity = m_registry.CreateEntity();
    m_registry.AddComponent(probeEntity, NameComponent{ "Reflection Probe" });

    ReflectionProbeComponent probeComp;
    probeComp.position = { 0.0f, 1.5f, 0.0f }; // 地面より少し高い位置（目の高さ）に配置
    probeComp.radius = 20.0f;                  // 影響範囲（今はまだ使いませんが設定しておく）
    probeComp.needsBake = true;                // 初回なので必ず撮影させる

    m_registry.AddComponent(probeEntity, probeComp);

    EntityID environmentEntity = m_registry.CreateEntity();
    m_registry.AddComponent(environmentEntity, NameComponent{ "Environment" });
    m_registry.AddComponent(environmentEntity, EnvironmentComponent{});
}

void GameLayer::Finalize()
{
 
}

void GameLayer::Update(const EngineTime& time)
{
    FreeCameraSystem::Update(m_registry, time.unscaledDt);


    TransformSystem transformSys;
    transformSys.Update(m_registry);

    ModelUpdateSystem::Update(m_registry);

    CameraFinalizeSystem::Update(m_registry);
}

void GameLayer::Render(RenderContext& rc, RenderQueue& queue)
{
    EnvironmentExtractSystem environmentExtractSystem;
    environmentExtractSystem.Extract(m_registry, rc);

    rc.bloomData.luminanceLowerEdge = m_postEffect.luminanceLowerEdge;
    rc.bloomData.luminanceHigherEdge = m_postEffect.luminanceHigherEdge;
    rc.bloomData.bloomIntensity = m_postEffect.bloomIntensity;
    rc.bloomData.gaussianSigma = m_postEffect.gaussianSigma;

    // カラーフィルター設定
    rc.colorFilterData.exposure = m_postEffect.exposure; // ★追加
    rc.colorFilterData.monoBlend = m_postEffect.monoBlend;
    rc.colorFilterData.hueShift = m_postEffect.hueShift;
    rc.colorFilterData.flashAmount = m_postEffect.flashAmount;
    rc.colorFilterData.vignetteAmount = m_postEffect.vignetteAmount;

    rc.dofData.enable = m_postEffect.enableDoF;
    rc.dofData.focusDistance = m_postEffect.focusDistance;
    rc.dofData.focusRange = m_postEffect.focusRange;
    rc.dofData.bokehRadius = m_postEffect.bokehRadius;

    rc.motionBlurData.intensity = m_postEffect.motionBlurIntensity;
    rc.motionBlurData.samples = static_cast<float>(m_postEffect.motionBlurSamples);
    
    rc.reflectionProbeTexture = nullptr;

    Query<ReflectionProbeComponent> probeQuery(m_registry);
    probeQuery.ForEach([&rc](ReflectionProbeComponent& probe) {
        if (!probe.cubemapTexture && Graphics::Instance().GetAPI() == GraphicsAPI::DX11 && probe.cubemapSRV) {
            // DX11 の既存ベイカーが作る SRV を、API 共通の ITexture 経路へ揃える。
            probe.cubemapTexture = std::make_shared<DX11Texture>(probe.cubemapSRV.Get());
        }

        if (probe.cubemapTexture) {
            rc.reflectionProbeTexture = probe.cubemapTexture.get();
        }
        });

    MeshExtractSystem extractSys;
    extractSys.Extract(m_registry, queue);
}


