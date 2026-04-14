#include "GameLayer.h"
#include "Graphics.h"
#include <Transform\TransformSystem.h>
#include <Transform\NodeAttachmentSystem.h>
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
#include "Input/InputContextSystem.h"
#include "Input/InputResolveSystem.h"
#include "Input/InputTextSystem.h"
#include "Input/InputFeedbackSystem.h"
#include "Engine/EngineKernel.h"
#include "Trail/TrailSystem.h"
#include "Trail/TrailExtractSystem.h"
#include "Gameplay/PlayerInputSystem.h"
#include "Gameplay/ActionSystem.h"
#include "Gameplay/DodgeSystem.h"
#include "Gameplay/LocomotionSystem.h"
#include "Gameplay/StaminaSystem.h"
#include "Gameplay/HealthSystem.h"
#include "Gameplay/CharacterPhysicsSystem.h"
#include "Gameplay/PlaybackSystem.h"
#include "Gameplay/StateMachineSystem.h"
#include "Gameplay/TimelineSystem.h"
#include "Gameplay/TimelineHitboxSystem.h"
#include "Gameplay/TimelineVFXSystem.h"
#include "Gameplay/TimelineAudioSystem.h"
#include "Gameplay/TimelineShakeSystem.h"
#include "Gameplay/HitboxTrackingSystem.h"
#include "EffectRuntime/EffectService.h"
#include "EffectRuntime/EffectSystems.h"
#include "Animator/AnimatorService.h"
#include "Animator/AnimatorSystem.h"
#include "Sequencer/CinematicService.h"
#include <Component\LightComponent.h>
#include "Component/EnvironmentComponent.h"
#include "Component/AudioSettingsComponent.h"
#include "Component/AudioListenerComponent.h"
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
    m_registry.AddComponent(cameraEntity, AudioListenerComponent{});

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

    EntityID audioSettingsEntity = m_registry.CreateEntity();
    m_registry.AddComponent(audioSettingsEntity, NameComponent{ "Audio Settings" });
    m_registry.AddComponent(audioSettingsEntity, AudioSettingsComponent{});
}

void GameLayer::Finalize()
{
 
}

void GameLayer::Update(const EngineTime& time)
{
    EffectService::Instance().SetRegistry(&m_registry);
    AnimatorService::Instance().SetRegistry(&m_registry);
    CinematicService::Instance().SetRegistry(&m_registry);

    auto& kernel = EngineKernel::Instance();
    const auto& eventQueue = kernel.GetInputEventQueue();

    InputContextSystem::Update(m_registry);
    InputResolveSystem::Update(m_registry, eventQueue, time.unscaledDt);
    InputTextSystem::Update(m_registry, eventQueue, kernel.GetInputBackend());
    InputFeedbackSystem::Update(m_registry, kernel.GetInputBackend(), time.unscaledDt);

    // --- Gameplay Systems (spec order) ---
    PlayerInputSystem::Update(m_registry);
    ActionSystem::Update(m_registry, time.dt);
    DodgeSystem::Update(m_registry, time.dt);
    LocomotionSystem::Update(m_registry, time.dt);
    StaminaSystem::Update(m_registry, time.dt);
    HealthSystem::Update(m_registry, time.dt);
    CharacterPhysicsSystem::Update(m_registry, time.dt);
    PlaybackSystem::Update(m_registry, time.dt);
    StateMachineSystem::Update(m_registry, time.dt);
    TimelineSystem::Update(m_registry);
    TimelineHitboxSystem::Update(m_registry);
    TimelineVFXSystem::Update(m_registry);
    TimelineAudioSystem::Update(m_registry);
    TimelineShakeSystem::Update(m_registry, time.dt);
    CinematicService::Instance().Update(time);
    EffectSpawnSystem::Update(m_registry, time.dt);
    EffectPlaybackSystem::Update(m_registry, time.dt);
    EffectSimulationSystem::Update(m_registry, time.dt);
    EffectLifetimeSystem::Update(m_registry, time.dt);
    const float previewDt = time.dt > 0.0f ? 0.0f : time.unscaledDt;
    EffectPreviewSystem::Update(m_registry, previewDt);
    HitboxTrackingSystem::Update(m_registry);

    FreeCameraSystem::Update(m_registry, time.unscaledDt);

    TransformSystem transformSys;
    transformSys.Update(m_registry);
    NodeAttachmentSystem::Update(m_registry);
    EffectAttachmentSystem::Update(m_registry);
    TrailSystem::Update(m_registry, time.dt);

    AnimatorSystem::Update(m_registry, time.dt);
    ModelUpdateSystem::Update(m_registry);

    CameraFinalizeSystem::Update(m_registry);
}

void GameLayer::Render(RenderContext& rc, RenderQueue& queue)
{
    EnvironmentExtractSystem environmentExtractSystem;
    environmentExtractSystem.Extract(m_registry, rc);

    rc.allowGpuDrivenCompute = m_postEffect.enableComputeCulling;
    rc.allowAsyncCompute = m_postEffect.enableComputeCulling && m_postEffect.enableAsyncCompute;
    rc.enableGTAO = m_postEffect.enableGTAO;
    rc.enableSSGI = m_postEffect.enableSSGI;
    rc.enableVolumetricFog = m_postEffect.enableVolumetricFog;
    rc.enableSSR = m_postEffect.enableSSR;

    rc.bloomData.luminanceLowerEdge = m_postEffect.enableBloom ? m_postEffect.luminanceLowerEdge : 0.0f;
    rc.bloomData.luminanceHigherEdge = m_postEffect.enableBloom ? m_postEffect.luminanceHigherEdge : 0.0f;
    rc.bloomData.bloomIntensity = m_postEffect.enableBloom ? m_postEffect.bloomIntensity : 0.0f;
    rc.bloomData.gaussianSigma = m_postEffect.enableBloom ? m_postEffect.gaussianSigma : 0.0f;

    // カラーフィルター設定
    rc.colorFilterData.exposure = m_postEffect.enableColorFilter ? m_postEffect.exposure : 0.0f;
    rc.colorFilterData.monoBlend = m_postEffect.enableColorFilter ? m_postEffect.monoBlend : 0.0f;
    rc.colorFilterData.hueShift = m_postEffect.enableColorFilter ? m_postEffect.hueShift : 0.0f;
    rc.colorFilterData.flashAmount = m_postEffect.enableColorFilter ? m_postEffect.flashAmount : 0.0f;
    rc.colorFilterData.vignetteAmount = m_postEffect.enableColorFilter ? m_postEffect.vignetteAmount : 0.0f;

    rc.dofData.enable = m_postEffect.enableDoF;
    rc.dofData.focusDistance = m_postEffect.focusDistance;
    rc.dofData.focusRange = m_postEffect.focusRange;
    rc.dofData.bokehRadius = m_postEffect.bokehRadius;

    rc.motionBlurData.intensity = m_postEffect.enableMotionBlur ? m_postEffect.motionBlurIntensity : 0.0f;
    rc.motionBlurData.samples = m_postEffect.enableMotionBlur ? static_cast<float>(m_postEffect.motionBlurSamples) : 0.0f;
    
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
    EffectExtractSystem::Extract(m_registry, rc, queue);
    TrailExtractSystem::Extract(m_registry, queue, rc);
}


