// 敵 Entity に必要なランタイムコンポーネントを付与・初期化する実装ファイル。
#include "EnemyRuntimeSetup.h"

#include "Archetype/Archetype.h"
#include "Component/ActorTypeComponent.h"
#include "Component/ColliderComponent.h"
#include "Component/ComponentSignature.h"
#include "Component/HierarchyComponent.h"
#include "Component/NameComponent.h"
#include "Component/TransformComponent.h"
#include "Gameplay/ActionStateComponent.h"
#include "Gameplay/CharacterPhysicsComponent.h"
#include "Gameplay/DodgeStateComponent.h"
#include "Gameplay/EnemyTagComponent.h"
#include "Gameplay/HealthComponent.h"
#include "Gameplay/HitStopComponent.h"
#include "Gameplay/HitboxTrackingComponent.h"
#include "Gameplay/HUDLinkComponent.h"
#include "Gameplay/LocomotionStateComponent.h"
#include "Gameplay/StateMachineAssetComponent.h"
#include "Gameplay/StateMachineParamsComponent.h"
#include "Gameplay/TeamComponent.h"
#include "Gameplay/TimelineComponent.h"
#include "Gameplay/TimelineItemBuffer.h"
#include "PlayerEditor/StateMachineAsset.h"
#include "Registry/Registry.h"
#include "Type/TypeInfo.h"

#include "AggroComponent.h"
#include "BehaviorTreeAssetComponent.h"
#include "BehaviorTreeRuntimeComponent.h"
#include "BlackboardComponent.h"
#include "EnemyConfigAsset.h"
#include "PerceptionComponent.h"

// このファイル内だけで使う補助関数と補助型を定義する無名名前空間。
namespace
{
    // 指定コンポーネントが無ければ追加し、追加後のポインタを返す。
    template<typename T>
    T* EnsureComponent(Registry& registry, EntityID entity)
    {
        if (auto* c = registry.GetComponent<T>(entity)) return c;
        registry.AddComponent(entity, T{});
        return registry.GetComponent<T>(entity);
    }
}

// 敵ランタイム構築用の関数群をまとめる名前空間。
namespace EnemyRuntimeSetup
{
    // 敵として動作するために必要な全ランタイムコンポーネントを揃える。
    void EnsureEnemyRuntimeComponents(Registry& registry, EntityID entity)
    {
        if (Entity::IsNull(entity)) return;

        EnsureComponent<EnemyTagComponent>(registry, entity);

        if (auto* actor = EnsureComponent<ActorTypeComponent>(registry, entity)) {
            actor->type = ActorType::Enemy;
        }

        EnsureComponent<TransformComponent>(registry, entity);
        EnsureComponent<HierarchyComponent>(registry, entity);
        EnsureComponent<HealthComponent>(registry, entity);
        EnsureComponent<CharacterPhysicsComponent>(registry, entity);
        EnsureComponent<HitStopComponent>(registry, entity);
        EnsureComponent<HitboxTrackingComponent>(registry, entity);
        EnsureComponent<TimelineComponent>(registry, entity);
        EnsureComponent<TimelineItemBuffer>(registry, entity);
        EnsureComponent<ActionStateComponent>(registry, entity);
        EnsureComponent<DodgeStateComponent>(registry, entity);
        EnsureComponent<StateMachineParamsComponent>(registry, entity);

        if (auto* team = EnsureComponent<TeamComponent>(registry, entity)) {
            team->teamId = 1;
        }
        if (auto* hudLink = EnsureComponent<HUDLinkComponent>(registry, entity)) {
            hudLink->asBossHUD    = true;
            hudLink->asWorldFloat = true;
        }

        if (auto* loco = EnsureComponent<LocomotionStateComponent>(registry, entity)) {
            loco->useCameraRelativeInput = false;
        }

        EnsureComponent<BehaviorTreeAssetComponent>(registry, entity);
        EnsureComponent<BehaviorTreeRuntimeComponent>(registry, entity);
        EnsureComponent<BlackboardComponent>(registry, entity);
        EnsureComponent<PerceptionComponent>(registry, entity);
        EnsureComponent<AggroComponent>(registry, entity);
    }

    // 敵 AI の一時状態・ヘイト・移動入力を初期化する。
    void ResetEnemyRuntimeState(Registry& registry, EntityID entity)
    {
        if (Entity::IsNull(entity)) return;
        if (auto* rt = registry.GetComponent<BehaviorTreeRuntimeComponent>(entity)) {
            rt->ResetAll();
        }
        if (auto* aggro = registry.GetComponent<AggroComponent>(entity)) {
            aggro->currentTarget   = Entity::NULL_ID;
            aggro->threat          = 0.0f;
            aggro->timeSinceSighted = 0.0f;
        }
        if (auto* bb = registry.GetComponent<BlackboardComponent>(entity)) {
            bb->entries.clear();
        }
        if (auto* loco = registry.GetComponent<LocomotionStateComponent>(entity)) {
            loco->moveInput   = { 0.0f, 0.0f };
            loco->inputStrength = 0.0f;
        }
    }

    // EnemyTag を持つ全 Entity に敵ランタイム構成を適用する。
    void EnsureAllEnemyRuntimeComponents(Registry& registry, bool resetRuntimeState)
    {
        Signature sig = CreateSignature<EnemyTagComponent>();
        std::vector<EntityID> enemies;
        for (auto* arch : registry.GetAllArchetypes()) {
            if (!SignatureMatches(arch->GetSignature(), sig)) continue;
            const auto& ents = arch->GetEntities();
            for (size_t i = 0; i < arch->GetEntityCount(); ++i) {
                enemies.push_back(ents[i]);
            }
        }
        for (EntityID e : enemies) {
            EnsureEnemyRuntimeComponents(registry, e);
            if (resetRuntimeState) ResetEnemyRuntimeState(registry, e);
        }
    }

    // NPC として動作するための最小ランタイム構成を揃える。
    void EnsureNPCRuntimeComponents(Registry& registry, EntityID entity)
    {
        if (Entity::IsNull(entity)) return;

        if (auto* actor = EnsureComponent<ActorTypeComponent>(registry, entity)) {
            actor->type = ActorType::NPC;
        }
        EnsureComponent<TransformComponent>(registry, entity);
        EnsureComponent<HierarchyComponent>(registry, entity);
        EnsureComponent<HealthComponent>(registry, entity);
        EnsureComponent<CharacterPhysicsComponent>(registry, entity);
        EnsureComponent<HitStopComponent>(registry, entity);
        EnsureComponent<HitboxTrackingComponent>(registry, entity);
        EnsureComponent<TimelineComponent>(registry, entity);
        EnsureComponent<TimelineItemBuffer>(registry, entity);
        EnsureComponent<ActionStateComponent>(registry, entity);
        EnsureComponent<DodgeStateComponent>(registry, entity);
        EnsureComponent<StateMachineParamsComponent>(registry, entity);
        if (auto* loco = EnsureComponent<LocomotionStateComponent>(registry, entity)) {
            loco->useCameraRelativeInput = false;
        }
        EnsureComponent<BehaviorTreeAssetComponent>(registry, entity);
        EnsureComponent<BehaviorTreeRuntimeComponent>(registry, entity);
        EnsureComponent<BlackboardComponent>(registry, entity);
    }

    // EnemyConfigAsset の内容から敵 Entity を生成して初期化する。
    EntityID SpawnFromConfig(Registry& registry,
                             const EnemyConfigAsset& config,
                             const DirectX::XMFLOAT3& position)
    {
        EntityID e = registry.CreateEntity();
        registry.AddComponent(e, NameComponent{ config.name.empty() ? "Enemy" : config.name });

        EnsureEnemyRuntimeComponents(registry, e);

        if (auto* tr = registry.GetComponent<TransformComponent>(e)) {
            tr->localPosition = position;
            tr->worldPosition = position;
        }

        if (auto* health = registry.GetComponent<HealthComponent>(e)) {
            health->maxHealth = static_cast<int>(config.maxHealth);
            health->health    = health->maxHealth;
        }

        if (auto* loco = registry.GetComponent<LocomotionStateComponent>(e)) {
            loco->walkMaxSpeed = config.walkSpeed;
            loco->jogMaxSpeed  = (config.walkSpeed + config.runSpeed) * 0.5f;
            loco->runMaxSpeed  = config.runSpeed;
            loco->turnSpeed    = config.turnSpeed;
        }

        if (auto* perception = registry.GetComponent<PerceptionComponent>(e)) {
            perception->sightRadius   = config.sightRadius;
            perception->sightFOV      = config.sightFOV;
            perception->hearingRadius = config.hearingRadius;
        }

        if (auto* btAsset = registry.GetComponent<BehaviorTreeAssetComponent>(e)) {
            btAsset->assetPath = config.behaviorTreePath;
        }

        return e;
    }
}


// このファイル内だけで使う補助関数と補助型を定義する無名名前空間。
namespace
{
    // 指定名の StateNode を取得し、無ければ新規作成する。
    StateNode* FindOrCreateState(StateMachineAsset& sm, const char* name, StateNodeType type)
    {
        for (auto& s : sm.states) {
            if (s.name == name) return &s;
        }
        return sm.AddState(name, type);
    }

    // 指定ステート間の遷移が無ければ追加する。
    void EnsureTransition(StateMachineAsset& sm, uint32_t fromId, uint32_t toId)
    {
        for (const auto& t : sm.transitions) {
            if (t.fromState == fromId && t.toState == toId) return;
        }
        sm.AddTransition(fromId, toId);
    }
}

// エディタ用に Enemy の標準構成と StateMachine を作成する。
void EnemyEditorSetupFullEnemy(Registry& registry, EntityID entity, StateMachineAsset& sm)
{
    if (Entity::IsNull(entity)) return;

    EnemyRuntimeSetup::EnsureEnemyRuntimeComponents(registry, entity);

    StateNode* idle    = FindOrCreateState(sm, "Idle",    StateNodeType::Locomotion);
    StateNode* chase   = FindOrCreateState(sm, "Chase",   StateNodeType::Locomotion);
    StateNode* attack1 = FindOrCreateState(sm, "Attack1", StateNodeType::Action);
    StateNode* damaged = FindOrCreateState(sm, "Damaged", StateNodeType::Damage);
    StateNode* dead    = FindOrCreateState(sm, "Dead",    StateNodeType::Dead);

    if (sm.defaultStateId == 0 && idle) sm.defaultStateId = idle->id;

    auto setBT = [&sm](StateNode* s, const char* leaf) {
        if (!s) return;
        if (s->behaviorTreePath.empty()) {
            const std::string base = sm.name.empty() ? std::string{ "Enemy" } : sm.name;
            s->behaviorTreePath = "Data/AI/BehaviorTrees/" + base + "_" + leaf + ".bt";
        }
    };
    setBT(idle,    "Idle");
    setBT(chase,   "Chase");
    (void)attack1; (void)damaged; (void)dead;

    if (idle && chase)   EnsureTransition(sm, idle->id, chase->id);
    if (chase && attack1)EnsureTransition(sm, chase->id, attack1->id);
    if (attack1 && idle) EnsureTransition(sm, attack1->id, idle->id);
    if (damaged && idle) EnsureTransition(sm, damaged->id, idle->id);
}

// エディタ操作から NPC Entity をフルセットアップする。
void EnemyEditorSetupFullNPC(Registry& registry, EntityID entity, StateMachineAsset& sm)
{
    if (Entity::IsNull(entity)) return;
    EnemyRuntimeSetup::EnsureNPCRuntimeComponents(registry, entity);
    StateNode* idle = FindOrCreateState(sm, "Idle", StateNodeType::Locomotion);
    if (idle && sm.defaultStateId == 0) sm.defaultStateId = idle->id;
}

// エディタ操作から敵ランタイムコンポーネントの不足を修復する。
void EnemyEditorRepairRuntime(Registry& registry, EntityID entity)
{
    if (Entity::IsNull(entity)) return;
    EnemyRuntimeSetup::EnsureEnemyRuntimeComponents(registry, entity);
}
