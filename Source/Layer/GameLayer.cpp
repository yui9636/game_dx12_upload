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
#include <Camera\ThirdPersonCameraSystem.h>
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
#include "AI/PerceptionSystem.h"
#include "AI/BehaviorTreeSystem.h"
#include "AI/EnemyRuntimeSetup.h"
#include "Gameplay/ActionSystem.h"
#include "Gameplay/DodgeSystem.h"
#include "Gameplay/LocomotionSystem.h"
#include "Gameplay/StaminaSystem.h"
#include "Gameplay/DamageSystem.h"
#include "Gameplay/DamageEventComponent.h"
#include "Gameplay/BattleFlowComponent.h"
#include "Gameplay/BattleFlowSystem.h"
#include "Gameplay/LockOnSystem.h"
#include "Gameplay/HealthSystem.h"
#include "UI/HUDBindingSystem.h"
#include "HeadUpDisplay.h"
#include "Gameplay/CharacterPhysicsSystem.h"
#include "Gameplay/PlaybackSystem.h"
#include "Gameplay/StateMachineSystem.h"
#include "Gameplay/TimelineSystem.h"
#include "Gameplay/TimelineHitboxSystem.h"
#include "Gameplay/TimelineVFXSystem.h"
#include "Gameplay/TimelineAudioSystem.h"
#include "Gameplay/TimelineShakeSystem.h"
#include "Gameplay/HitboxTrackingSystem.h"
#include "Collision/CollisionSystem.h"
#include "DebugRender/DebugRenderSystem.h"
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
#include "Sprite/Sprite.h"
#include "UI/UIHPNumber.h"
#include "UI/UIHPText2D.h"
#include "UI/UIManager.h"
#include "UI/UI2DSpriteExtractSystem.h"
#include "UI/UIProgressBar2D.h"
#include "UI/UIProgressBar3D.h"
#include <algorithm>
#include <unordered_set>
#include <vector>

namespace
{
    bool IsLegacyRuntimeSingletonName(const std::string& name)
    {
        return name == "_DamageEventQueue" || name == "_BattleFlow";
    }

    void AddUniqueEntity(std::vector<EntityID>& entities, EntityID entity)
    {
        if (Entity::IsNull(entity)) {
            return;
        }
        if (std::find(entities.begin(), entities.end(), entity) == entities.end()) {
            entities.push_back(entity);
        }
    }

    void RemoveLegacyRuntimeSingletonEntities(Registry& registry)
    {
        std::vector<EntityID> toDestroy;

        {
            Query<DamageEventComponent> q(registry);
            q.ForEachWithEntity([&](EntityID entity, DamageEventComponent&) {
                AddUniqueEntity(toDestroy, entity);
            });
        }
        {
            Query<BattleFlowComponent> q(registry);
            q.ForEachWithEntity([&](EntityID entity, BattleFlowComponent&) {
                AddUniqueEntity(toDestroy, entity);
            });
        }
        {
            Query<NameComponent> q(registry);
            q.ForEachWithEntity([&](EntityID entity, NameComponent& name) {
                if (IsLegacyRuntimeSingletonName(name.name)) {
                    AddUniqueEntity(toDestroy, entity);
                }
            });
        }

        for (EntityID entity : toDestroy) {
            registry.DestroyEntity(entity);
        }
    }

    DirectX::XMFLOAT4 ResolveHPFillColor(float ratio)
    {
        if (ratio <= 0.25f) {
            return { 0.95f, 0.16f, 0.12f, 0.94f };
        }
        if (ratio <= 0.5f) {
            return { 0.95f, 0.72f, 0.16f, 0.94f };
        }
        return { 0.18f, 0.86f, 0.36f, 0.94f };
    }

    void ApplyColor(const std::shared_ptr<UIElement>& element, const DirectX::XMFLOAT4& c)
    {
        if (element) {
            element->SetColor(c.x, c.y, c.z, c.w);
        }
    }
}

void GameLayer::Initialize()
{
    DamageEventRuntimeQueue::Clear();
    BattleFlowSystem::Reset();

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

    RemoveLegacyRuntimeSingletonEntities(m_registry);
    InitializeHUD();
}

void GameLayer::Finalize()
{
    ShutdownHUD();
}

void GameLayer::Update(const EngineTime& time)
{
    RemoveLegacyRuntimeSingletonEntities(m_registry);

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
    // Make sure Enemy entities have all AI components (cheap; only runs when needed).
    EnemyRuntimeSetup::EnsureAllEnemyRuntimeComponents(m_registry, false);

    // AI: perception (write Aggro / Blackboard) before BT decision.
    PerceptionSystem::Update(m_registry, time.dt);

    // Player input (writes SM params for player-tagged entities).
    PlayerInputSystem::Update(m_registry);

    // AI decision (writes SM params + locomotion for enemies).
    BehaviorTreeSystem::Update(m_registry, time.dt);

    PlaybackSystem::Update(m_registry, time.dt);
    StateMachineSystem::Update(m_registry, time.dt);
    LocomotionSystem::Update(m_registry, time.dt);
    StaminaSystem::Update(m_registry, time.dt);
    // HealthSystem moved below — it must run after DamageSystem so events
    // produced this frame are applied the same frame.
    DodgeSystem::Update(m_registry, time.dt);
    CharacterPhysicsSystem::Update(m_registry, time.dt);
    TimelineSystem::Update(m_registry);
    ActionSystem::Update(m_registry, time.dt);
    CinematicService::Instance().Update(time);
    EffectSpawnSystem::Update(m_registry, time.dt);
    EffectPlaybackSystem::Update(m_registry, time.dt);
    EffectSimulationSystem::Update(m_registry, time.dt);
    EffectLifetimeSystem::Update(m_registry, time.dt);
    const float previewDt = time.dt > 0.0f ? 0.0f : time.unscaledDt;
    EffectPreviewSystem::Update(m_registry, previewDt);

    FreeCameraSystem::Update(m_registry, time.unscaledDt);
    // Lock-on selects/clears the camera target before the third-person follow
    // pulls a position from CameraTPVControlComponent.target.
    LockOnSystem::Update(m_registry, time.dt);
    ThirdPersonCameraSystem::Update(m_registry, time.dt);

    TransformSystem transformSys;

    transformSys.Update(m_registry);

    AnimatorSystem::Update(m_registry, time.dt);

    transformSys.Update(m_registry);

    ModelUpdateSystem::Update(m_registry);

    NodeAttachmentSystem::Update(m_registry);
    TimelineHitboxSystem::Update(m_registry);
    TimelineVFXSystem::Update(m_registry);
    TimelineAudioSystem::Update(m_registry);
    TimelineShakeSystem::Update(m_registry, time.dt);
    EffectAttachmentSystem::Update(m_registry, time.dt);
    TrailSystem::Update(m_registry, time.dt);
    HitboxTrackingSystem::Update(m_registry);

    // Body colliders are synced to the manager here. TimelineHitboxSystem
    // already pushed the active Attack colliders above, so once we sync the
    // bodies the (Attack vs Body) pairs are visible to ComputeAllContacts.
    CollisionSystem collisionSystem;
    collisionSystem.Update(m_registry);

    DamageSystem::Update(m_registry);
    HealthSystem::Update(m_registry, time.dt);

    HUDBindingSystem::Update(m_registry);
    ApplyHUDState();
    BattleFlowSystem::Update(m_registry, time.dt);

    CameraFinalizeSystem::Update(m_registry);
}

void GameLayer::InitializeHUD()
{
    auto& ui = UIManager::Instance();

    m_hudWhiteSprite = std::make_shared<Sprite>("Data/Texture/UI/White.png");

    const auto setupBar = [&](std::shared_ptr<UIProgressBar2D>& bar,
                              const DirectX::XMFLOAT4& color,
                              float centerXNorm,
                              float centerYNorm,
                              float widthNorm,
                              float heightPx) {
        bar = ui.CreateElement<UIProgressBar2D>();
        bar->SetSprite(m_hudWhiteSprite);
        bar->SetResponsiveRect(centerXNorm, centerYNorm, widthNorm, heightPx);
        bar->SetProgress(1.0f);
        bar->SetColor(color.x, color.y, color.z, color.w);
        bar->SetVisible(false);
    };

    setupBar(m_playerBarBackground, { 0.02f, 0.02f, 0.025f, 0.72f }, 0.50f, 0.925f, 0.42f, 18.0f);
    setupBar(m_playerBarFill,       { 0.18f, 0.86f, 0.36f, 0.94f },  0.50f, 0.925f, 0.42f, 18.0f);
    setupBar(m_bossBarBackground,   { 0.02f, 0.02f, 0.025f, 0.72f }, 0.50f, 0.075f, 0.55f, 16.0f);
    setupBar(m_bossBarFill,         { 0.95f, 0.16f, 0.12f, 0.94f },  0.50f, 0.075f, 0.55f, 16.0f);

    m_playerHPText = ui.CreateElement<UIHPText2D>();
    m_playerHPText->SetResponsivePosition(0.50f, 0.895f);
    m_playerHPText->SetScale(0.28f);
    m_playerHPText->SetColor(1.0f, 1.0f, 1.0f, 0.96f);
    m_playerHPText->SetVisible(false);

    m_bossHPText = ui.CreateElement<UIHPText2D>();
    m_bossHPText->SetResponsivePosition(0.50f, 0.105f);
    m_bossHPText->SetScale(0.25f);
    m_bossHPText->SetColor(1.0f, 1.0f, 1.0f, 0.94f);
    m_bossHPText->SetVisible(false);

    m_headUpDisplay = ui.CreateElement<HeadUpDisplay>();
}

void GameLayer::ShutdownHUD()
{
    auto& ui = UIManager::Instance();
    const auto removeElement = [&ui](auto& element) {
        if (element) {
            ui.RemoveElement(element);
            element.reset();
        }
    };

    removeElement(m_playerBarBackground);
    removeElement(m_playerBarFill);
    removeElement(m_playerHPText);
    removeElement(m_bossBarBackground);
    removeElement(m_bossBarFill);
    removeElement(m_bossHPText);
    removeElement(m_headUpDisplay);

    for (auto& [entity, item] : m_worldHUD) {
        (void)entity;
        if (item.bar) {
            ui.RemoveElement(item.bar);
        }
        if (item.text) {
            ui.RemoveElement(item.text);
        }
    }
    m_worldHUD.clear();
    m_hudWhiteSprite.reset();
}

void GameLayer::ApplyHUDState()
{
    const auto& state = HUDBindingSystem::GetState();

    const auto applyFlatHUD = [](bool active,
                                 const std::shared_ptr<UIProgressBar2D>& background,
                                 const std::shared_ptr<UIProgressBar2D>& fill,
                                 const std::shared_ptr<UIHPText2D>& text,
                                 float ratio,
                                 int hp,
                                 int maxHP) {
        if (background) {
            background->SetVisible(active);
        }
        if (fill) {
            fill->SetVisible(active);
            fill->SetProgress(ratio);
            const DirectX::XMFLOAT4 c = ResolveHPFillColor(ratio);
            fill->SetColor(c.x, c.y, c.z, c.w);
        }
        if (text) {
            text->SetVisible(active);
            text->SetHP(hp, maxHP);
        }
    };

    applyFlatHUD(state.playerActive, m_playerBarBackground, m_playerBarFill, m_playerHPText,
                 state.playerRatio, state.playerHP, state.playerMaxHP);
    applyFlatHUD(state.bossActive, m_bossBarBackground, m_bossBarFill, m_bossHPText,
                 state.bossRatio, state.bossHP, state.bossMaxHP);

    std::unordered_set<EntityID> activeWorldEntities;
    auto& ui = UIManager::Instance();
    for (const auto& entry : state.world) {
        if (Entity::IsNull(entry.entity) || entry.maxHP <= 0) {
            continue;
        }

        activeWorldEntities.insert(entry.entity);
        auto& item = m_worldHUD[entry.entity];
        if (!item.bar) {
            item.bar = ui.CreateElement<UIProgressBar3D>();
            item.bar->SetSprite(m_hudWhiteSprite);
            item.bar->SetBackgroundSprite(m_hudWhiteSprite);
            item.bar->SetBackgroundColor(0.02f, 0.02f, 0.025f, 0.72f);
            item.bar->SetSize(1.15f, 0.10f);
        }
        if (!item.text) {
            item.text = ui.CreateElement<UIHPNumber>();
            item.text->SetScreenOffset(0.0f, -18.0f);
            item.text->SetScale(0.18f);
        }

        item.bar->SetVisible(true);
        item.bar->SetPosition(entry.worldPos);
        item.bar->SetProgress(entry.ratio);
        ApplyColor(item.bar, ResolveHPFillColor(entry.ratio));

        item.text->SetVisible(true);
        item.text->SetPosition(entry.worldPos);
        item.text->SetHP(entry.hp, entry.maxHP);
        item.text->SetColor(1.0f, 1.0f, 1.0f, 0.92f);
    }

    for (auto it = m_worldHUD.begin(); it != m_worldHUD.end();) {
        if (activeWorldEntities.find(it->first) == activeWorldEntities.end()) {
            if (it->second.bar) {
                ui.RemoveElement(it->second.bar);
            }
            if (it->second.text) {
                ui.RemoveElement(it->second.text);
            }
            it = m_worldHUD.erase(it);
        } else {
            ++it;
        }
    }
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
    UI2DSpriteExtractSystem::Extract(m_registry, queue);

    DebugRenderSystem debugRenderSystem;
    debugRenderSystem.Render(m_registry);
}


