#include "BehaviorTreeSystem.h"

#include <cmath>
#include <memory>
#include <unordered_map>
#include <vector>

#include <DirectXMath.h>

#include "Archetype/Archetype.h"
#include "Component/ActorTypeComponent.h"
#include "Component/ComponentSignature.h"
#include "Component/TransformComponent.h"
#include "Engine/EngineKernel.h"
#include "Engine/EngineMode.h"
#include "Gameplay/ActionStateComponent.h"
#include "Gameplay/EnemyTagComponent.h"
#include "Gameplay/HealthComponent.h"
#include "Gameplay/LocomotionStateComponent.h"
#include "Gameplay/StateMachineAssetComponent.h"
#include "Gameplay/StateMachineParamsComponent.h"
#include "Registry/Registry.h"
#include "Type/TypeInfo.h"

#include "AggroComponent.h"
#include "BehaviorTreeAsset.h"
#include "BehaviorTreeAssetComponent.h"
#include "BehaviorTreeRuntimeComponent.h"
#include "BlackboardComponent.h"
#include "PerceptionComponent.h"

// ============================================================================
// Asset cache (path -> loaded asset)
// ============================================================================
namespace
{
    std::unordered_map<std::string, std::shared_ptr<BehaviorTreeAsset>>& AssetCache()
    {
        static std::unordered_map<std::string, std::shared_ptr<BehaviorTreeAsset>> cache;
        return cache;
    }

    const BehaviorTreeAsset* LoadOrCache(const std::string& path)
    {
        if (path.empty()) return nullptr;
        auto& cache = AssetCache();
        auto it = cache.find(path);
        if (it != cache.end()) return it->second.get();
        auto asset = std::make_shared<BehaviorTreeAsset>();
        if (!asset->LoadFromFile(path)) {
            return nullptr;
        }
        cache.emplace(path, asset);
        return cache[path].get();
    }
}

// ============================================================================
// Tick context + status
// ============================================================================
namespace
{
    enum class BTStatus : uint8_t
    {
        None    = 0,
        Running = 1,
        Success = 2,
        Failure = 3,
    };

    struct BTContext
    {
        Registry&                     registry;
        EntityID                      self;
        TransformComponent*           selfTransform;
        LocomotionStateComponent*     loco;
        ActionStateComponent*         actionState;
        StateMachineParamsComponent*  smParams;
        HealthComponent*              health;
        AggroComponent*               aggro;
        PerceptionComponent*          perception;
        BlackboardComponent*          blackboard;
        BehaviorTreeRuntimeComponent& runtime;
        float                         dt;
    };

    BTStatus Tick(const BehaviorTreeAsset& asset, const BTNode& node, BTContext& ctx);

    BTStatus TickChild(const BehaviorTreeAsset& asset, uint32_t childId, BTContext& ctx)
    {
        const BTNode* child = asset.FindNode(childId);
        if (!child) return BTStatus::Failure;
        return Tick(asset, *child, ctx);
    }

    DirectX::XMFLOAT3 ReadTargetPos(const BTContext& ctx, bool& outOk)
    {
        outOk = false;
        if (!ctx.aggro) return { 0.0f, 0.0f, 0.0f };
        if (Entity::IsNull(ctx.aggro->currentTarget)) return { 0.0f, 0.0f, 0.0f };
        auto* tr = ctx.registry.GetComponent<TransformComponent>(ctx.aggro->currentTarget);
        if (!tr) return { 0.0f, 0.0f, 0.0f };
        outOk = true;
        return tr->worldPosition;
    }

    float DistanceToTargetXZ(const BTContext& ctx, bool& outOk)
    {
        outOk = false;
        if (!ctx.selfTransform) return 0.0f;
        DirectX::XMFLOAT3 tgt = ReadTargetPos(ctx, outOk);
        if (!outOk) return 0.0f;
        const float dx = tgt.x - ctx.selfTransform->worldPosition.x;
        const float dz = tgt.z - ctx.selfTransform->worldPosition.z;
        return std::sqrt(dx * dx + dz * dz);
    }

    void WriteWorldMove(BTContext& ctx, float dirX, float dirZ, float strength)
    {
        if (!ctx.loco) return;
        const float len = std::sqrt(dirX * dirX + dirZ * dirZ);
        if (len < 0.0001f) {
            ctx.loco->moveInput     = { 0.0f, 0.0f };
            ctx.loco->inputStrength = 0.0f;
            return;
        }
        ctx.loco->moveInput     = { dirX / len, dirZ / len };
        ctx.loco->inputStrength = strength;
        ctx.loco->targetAngleY  = std::atan2(-dirZ / len, dirX / len);
    }
}

// ============================================================================
// Per-type tick functions
// ============================================================================
namespace
{
    BTStatus TickRoot(const BehaviorTreeAsset& asset, const BTNode& node, BTContext& ctx)
    {
        if (node.childrenIds.empty()) return BTStatus::Failure;
        return TickChild(asset, node.childrenIds[0], ctx);
    }

    BTStatus TickSequence(const BehaviorTreeAsset& asset, const BTNode& node, BTContext& ctx)
    {
        for (uint32_t cid : node.childrenIds) {
            BTStatus s = TickChild(asset, cid, ctx);
            if (s == BTStatus::Failure) return BTStatus::Failure;
            if (s == BTStatus::Running) return BTStatus::Running;
        }
        return BTStatus::Success;
    }

    BTStatus TickSelector(const BehaviorTreeAsset& asset, const BTNode& node, BTContext& ctx)
    {
        for (uint32_t cid : node.childrenIds) {
            BTStatus s = TickChild(asset, cid, ctx);
            if (s == BTStatus::Success) return BTStatus::Success;
            if (s == BTStatus::Running) return BTStatus::Running;
        }
        return BTStatus::Failure;
    }

    BTStatus TickParallel(const BehaviorTreeAsset& asset, const BTNode& node, BTContext& ctx)
    {
        if (node.childrenIds.empty()) return BTStatus::Failure;
        const int threshold = node.iParam0 < 1 ? 1 : node.iParam0;
        int successes = 0;
        int failures  = 0;
        for (uint32_t cid : node.childrenIds) {
            BTStatus s = TickChild(asset, cid, ctx);
            if (s == BTStatus::Success) ++successes;
            else if (s == BTStatus::Failure) ++failures;
        }
        if (successes >= threshold) return BTStatus::Success;
        const int total = static_cast<int>(node.childrenIds.size());
        if (failures > (total - threshold)) return BTStatus::Failure;
        return BTStatus::Running;
    }

    BTStatus TickInverter(const BehaviorTreeAsset& asset, const BTNode& node, BTContext& ctx)
    {
        if (node.childrenIds.empty()) return BTStatus::Failure;
        BTStatus s = TickChild(asset, node.childrenIds[0], ctx);
        if (s == BTStatus::Success) return BTStatus::Failure;
        if (s == BTStatus::Failure) return BTStatus::Success;
        return s;
    }

    BTStatus TickRepeat(const BehaviorTreeAsset& asset, const BTNode& node, BTContext& ctx)
    {
        if (node.childrenIds.empty() || node.iParam0 <= 0) return BTStatus::Failure;
        const float doneCount = ctx.runtime.GetNodeState(node.id);
        BTStatus s = TickChild(asset, node.childrenIds[0], ctx);
        if (s == BTStatus::Failure) {
            ctx.runtime.ClearNodeState(node.id);
            return BTStatus::Failure;
        }
        if (s == BTStatus::Success) {
            const float next = doneCount + 1.0f;
            if (static_cast<int>(next) >= node.iParam0) {
                ctx.runtime.ClearNodeState(node.id);
                return BTStatus::Success;
            }
            ctx.runtime.SetNodeState(node.id, next);
            return BTStatus::Running;
        }
        return BTStatus::Running;
    }

    BTStatus TickCooldown(const BehaviorTreeAsset& asset, const BTNode& node, BTContext& ctx)
    {
        if (node.childrenIds.empty() || node.fParam0 <= 0.0f) return BTStatus::Failure;
        // Stored value: remaining cooldown seconds.
        const float remaining = ctx.runtime.GetNodeState(node.id);
        if (remaining > 0.0f) {
            const float next = remaining - ctx.dt;
            if (next > 0.0f) {
                ctx.runtime.SetNodeState(node.id, next);
                return BTStatus::Failure;
            }
            ctx.runtime.ClearNodeState(node.id);
        }
        BTStatus s = TickChild(asset, node.childrenIds[0], ctx);
        if (s == BTStatus::Success) {
            ctx.runtime.SetNodeState(node.id, node.fParam0);
        }
        return s;
    }

    BTStatus TickConditionGuard(const BehaviorTreeAsset& asset, const BTNode& node, BTContext& ctx)
    {
        // Use sParam0 as condition node id (string-encoded).
        // Simpler: treat first child as the guarded subtree, second child as the condition.
        // To stay strict: sParam0 holds the condition node id as a decimal string;
        // if empty, fall back to "evaluate child[0] only when blackboard 'TargetDist' != 0".
        if (node.childrenIds.empty()) return BTStatus::Failure;

        bool conditionOk = true;
        if (!node.sParam0.empty()) {
            try {
                const uint32_t condId = static_cast<uint32_t>(std::stoul(node.sParam0));
                BTStatus cs = TickChild(asset, condId, ctx);
                conditionOk = (cs == BTStatus::Success);
            } catch (...) {
                conditionOk = false;
            }
        }
        if (!conditionOk) return BTStatus::Failure;
        return TickChild(asset, node.childrenIds[0], ctx);
    }

    // ---- Conditions ----
    BTStatus TickHasTarget(const BTNode&, BTContext& ctx)
    {
        if (!ctx.aggro) return BTStatus::Failure;
        return Entity::IsNull(ctx.aggro->currentTarget) ? BTStatus::Failure : BTStatus::Success;
    }

    BTStatus TickTargetInRange(const BTNode& node, BTContext& ctx)
    {
        bool ok = false;
        const float d = DistanceToTargetXZ(ctx, ok);
        if (!ok) return BTStatus::Failure;
        return d <= node.fParam0 ? BTStatus::Success : BTStatus::Failure;
    }

    BTStatus TickTargetVisible(const BTNode&, BTContext& ctx)
    {
        if (!ctx.blackboard) return BTStatus::Failure;
        auto it = ctx.blackboard->entries.find("LastSeenTime");
        if (it == ctx.blackboard->entries.end()) return BTStatus::Failure;
        return (it->second.f <= 0.0001f) ? BTStatus::Success : BTStatus::Failure;
    }

    BTStatus TickHealthBelow(const BTNode& node, BTContext& ctx)
    {
        if (!ctx.health || ctx.health->maxHealth <= 0) return BTStatus::Failure;
        const float ratio = static_cast<float>(ctx.health->health) / static_cast<float>(ctx.health->maxHealth);
        return ratio < node.fParam0 ? BTStatus::Success : BTStatus::Failure;
    }

    BTStatus TickStaminaAbove(const BTNode& /*node*/, BTContext& /*ctx*/)
    {
        // StaminaComponent is optional; v1 returns Failure when absent.
        return BTStatus::Failure;
    }

    BTStatus TickBlackboardEqual(const BTNode& node, BTContext& ctx)
    {
        if (!ctx.blackboard || node.sParam0.empty()) return BTStatus::Failure;
        auto it = ctx.blackboard->entries.find(node.sParam0);
        if (it == ctx.blackboard->entries.end()) return BTStatus::Failure;
        const auto& v = it->second;
        switch (node.bbType) {
        case BlackboardValueType::Bool:    return (v.i != 0) == (node.iParam0 != 0) ? BTStatus::Success : BTStatus::Failure;
        case BlackboardValueType::Int:     return v.i == node.iParam0 ? BTStatus::Success : BTStatus::Failure;
        case BlackboardValueType::Float:   return std::fabs(v.f - node.fParam0) < 0.0001f ? BTStatus::Success : BTStatus::Failure;
        case BlackboardValueType::String:  return v.s == node.sParam1 ? BTStatus::Success : BTStatus::Failure;
        default:                            return BTStatus::Failure;
        }
    }

    // ---- Actions: locomotion ----
    BTStatus TickWait(const BTNode& node, BTContext& ctx)
    {
        if (node.fParam0 <= 0.0f) return BTStatus::Success;
        const float elapsed = ctx.runtime.GetNodeState(node.id) + ctx.dt;
        if (elapsed >= node.fParam0) {
            ctx.runtime.ClearNodeState(node.id);
            return BTStatus::Success;
        }
        ctx.runtime.SetNodeState(node.id, elapsed);
        return BTStatus::Running;
    }

    BTStatus TickFaceTarget(const BTNode&, BTContext& ctx)
    {
        bool ok = false;
        DirectX::XMFLOAT3 tgt = ReadTargetPos(ctx, ok);
        if (!ok || !ctx.loco || !ctx.selfTransform) return BTStatus::Failure;
        const float dx = tgt.x - ctx.selfTransform->worldPosition.x;
        const float dz = tgt.z - ctx.selfTransform->worldPosition.z;
        const float len = std::sqrt(dx * dx + dz * dz);
        if (len > 0.0001f) {
            ctx.loco->targetAngleY = std::atan2(-dz / len, dx / len);
        }
        return BTStatus::Success;
    }

    BTStatus TickMoveToTarget(const BTNode& node, BTContext& ctx)
    {
        bool ok = false;
        DirectX::XMFLOAT3 tgt = ReadTargetPos(ctx, ok);
        if (!ok || !ctx.loco || !ctx.selfTransform) {
            if (ctx.loco) {
                ctx.loco->moveInput     = { 0.0f, 0.0f };
                ctx.loco->inputStrength = 0.0f;
            }
            return BTStatus::Failure;
        }
        const float dx = tgt.x - ctx.selfTransform->worldPosition.x;
        const float dz = tgt.z - ctx.selfTransform->worldPosition.z;
        const float dist = std::sqrt(dx * dx + dz * dz);
        if (dist <= node.fParam0) {
            ctx.loco->moveInput     = { 0.0f, 0.0f };
            ctx.loco->inputStrength = 0.0f;
            return BTStatus::Success;
        }
        WriteWorldMove(ctx, dx, dz, 1.0f);
        return BTStatus::Running;
    }

    BTStatus TickStrafeAroundTarget(const BTNode& node, BTContext& ctx)
    {
        bool ok = false;
        DirectX::XMFLOAT3 tgt = ReadTargetPos(ctx, ok);
        if (!ok || !ctx.loco || !ctx.selfTransform) return BTStatus::Failure;

        const float dx = tgt.x - ctx.selfTransform->worldPosition.x;
        const float dz = tgt.z - ctx.selfTransform->worldPosition.z;
        const float len = std::sqrt(dx * dx + dz * dz);
        if (len < 0.0001f) return BTStatus::Failure;
        // Tangent: perpendicular to (dx, dz), counter-clockwise.
        const float tx = -dz;
        const float tz =  dx;
        WriteWorldMove(ctx, tx, tz, 0.6f);
        ctx.loco->targetAngleY = std::atan2(-dz / len, dx / len);

        const float elapsed = ctx.runtime.GetNodeState(node.id) + ctx.dt;
        if (elapsed >= node.fParam0) {
            ctx.runtime.ClearNodeState(node.id);
            ctx.loco->moveInput     = { 0.0f, 0.0f };
            ctx.loco->inputStrength = 0.0f;
            return BTStatus::Success;
        }
        ctx.runtime.SetNodeState(node.id, elapsed);
        return BTStatus::Running;
    }

    BTStatus TickRetreat(const BTNode& node, BTContext& ctx)
    {
        bool ok = false;
        DirectX::XMFLOAT3 tgt = ReadTargetPos(ctx, ok);
        if (!ok || !ctx.loco || !ctx.selfTransform) return BTStatus::Failure;
        const float dx = ctx.selfTransform->worldPosition.x - tgt.x;
        const float dz = ctx.selfTransform->worldPosition.z - tgt.z;
        const float dist = std::sqrt(dx * dx + dz * dz);
        if (dist >= node.fParam0) {
            ctx.loco->moveInput     = { 0.0f, 0.0f };
            ctx.loco->inputStrength = 0.0f;
            return BTStatus::Success;
        }
        WriteWorldMove(ctx, dx, dz, 1.0f);
        return BTStatus::Running;
    }

    // ---- Actions: combat (rising-edge) ----
    // phase: 0=Idle, 1=Requested, 2=InProgress
    BTStatus TickRisingEdgeAction(const BTNode& node, BTContext& ctx,
                                  const char* paramName,
                                  CharacterState targetState)
    {
        if (!ctx.actionState || !ctx.smParams) return BTStatus::Failure;
        const float phaseF = ctx.runtime.GetNodeState(node.id);
        const int phase = static_cast<int>(phaseF + 0.5f);

        if (ctx.actionState->state == CharacterState::Damage ||
            ctx.actionState->state == CharacterState::Dead) {
            ctx.runtime.ClearNodeState(node.id);
            return BTStatus::Failure;
        }

        if (phase == 0) {
            if (ctx.actionState->state != CharacterState::Locomotion) {
                return BTStatus::Failure;
            }
            ctx.smParams->SetParam(paramName, 1.0f);
            ctx.runtime.SetNodeState(node.id, 1.0f);
            return BTStatus::Running;
        }

        if (phase == 1) {
            if (ctx.actionState->state == targetState) {
                ctx.runtime.SetNodeState(node.id, 2.0f);
                return BTStatus::Running;
            }
            if (ctx.actionState->state == CharacterState::Locomotion) {
                // SM did not transition yet; keep trying for a few ticks.
                ctx.smParams->SetParam(paramName, 1.0f);
                return BTStatus::Running;
            }
            return BTStatus::Running;
        }

        // phase 2: in progress. Wait for return to Locomotion.
        if (ctx.actionState->state == CharacterState::Locomotion) {
            ctx.runtime.ClearNodeState(node.id);
            return BTStatus::Success;
        }
        return BTStatus::Running;
    }

    BTStatus TickAttack(const BTNode& node, BTContext& ctx)
    {
        return TickRisingEdgeAction(node, ctx, "Attack", CharacterState::Action);
    }

    BTStatus TickDodgeAction(const BTNode& node, BTContext& ctx)
    {
        return TickRisingEdgeAction(node, ctx, "Dodge", CharacterState::Dodge);
    }

    // ---- Actions: state-machine I/F ----
    BTStatus TickSetSMParam(const BTNode& node, BTContext& ctx)
    {
        if (!ctx.smParams || node.sParam0.empty()) return BTStatus::Failure;
        ctx.smParams->SetParam(node.sParam0.c_str(), node.fParam0);
        return BTStatus::Success;
    }

    BTStatus TickPlayState(const BTNode&, BTContext&)
    {
        // PlayState writes currentStateId by name. v1 implements as Success no-op
        // (state-name -> id resolution is StateMachineSystem's domain).
        return BTStatus::Success;
    }

    BTStatus TickSetBlackboard(const BTNode& node, BTContext& ctx)
    {
        if (!ctx.blackboard || node.sParam1.empty()) return BTStatus::Failure;
        BlackboardValue v;
        v.type = node.bbType;
        switch (node.bbType) {
        case BlackboardValueType::Bool:    v.i = node.iParam0; break;
        case BlackboardValueType::Int:     v.i = node.iParam0; break;
        case BlackboardValueType::Float:   v.f = node.fParam0; break;
        case BlackboardValueType::String:  v.s = node.sParam0; break;
        default: break;
        }
        ctx.blackboard->entries[node.sParam1] = v;
        return BTStatus::Success;
    }

    BTStatus Tick(const BehaviorTreeAsset& asset, const BTNode& node, BTContext& ctx)
    {
        BTStatus s = BTStatus::Failure;
        switch (node.type) {
        case BTNodeType::Root:               s = TickRoot(asset, node, ctx); break;
        case BTNodeType::Sequence:           s = TickSequence(asset, node, ctx); break;
        case BTNodeType::Selector:           s = TickSelector(asset, node, ctx); break;
        case BTNodeType::Parallel:           s = TickParallel(asset, node, ctx); break;
        case BTNodeType::Inverter:           s = TickInverter(asset, node, ctx); break;
        case BTNodeType::Repeat:             s = TickRepeat(asset, node, ctx); break;
        case BTNodeType::Cooldown:           s = TickCooldown(asset, node, ctx); break;
        case BTNodeType::ConditionGuard:     s = TickConditionGuard(asset, node, ctx); break;
        case BTNodeType::HasTarget:          s = TickHasTarget(node, ctx); break;
        case BTNodeType::TargetInRange:      s = TickTargetInRange(node, ctx); break;
        case BTNodeType::TargetVisible:      s = TickTargetVisible(node, ctx); break;
        case BTNodeType::HealthBelow:        s = TickHealthBelow(node, ctx); break;
        case BTNodeType::StaminaAbove:       s = TickStaminaAbove(node, ctx); break;
        case BTNodeType::BlackboardEqual:    s = TickBlackboardEqual(node, ctx); break;
        case BTNodeType::Wait:               s = TickWait(node, ctx); break;
        case BTNodeType::FaceTarget:         s = TickFaceTarget(node, ctx); break;
        case BTNodeType::MoveToTarget:       s = TickMoveToTarget(node, ctx); break;
        case BTNodeType::StrafeAroundTarget: s = TickStrafeAroundTarget(node, ctx); break;
        case BTNodeType::Retreat:            s = TickRetreat(node, ctx); break;
        case BTNodeType::Attack:             s = TickAttack(node, ctx); break;
        case BTNodeType::DodgeAction:        s = TickDodgeAction(node, ctx); break;
        case BTNodeType::SetSMParam:         s = TickSetSMParam(node, ctx); break;
        case BTNodeType::PlayState:          s = TickPlayState(node, ctx); break;
        case BTNodeType::SetBlackboard:      s = TickSetBlackboard(node, ctx); break;
        }
        ctx.runtime.PushDebugTrace(node.id, static_cast<uint8_t>(s));
        return s;
    }
}

void BehaviorTreeSystem::InvalidateAssetCache(const char* path)
{
    auto& cache = AssetCache();
    if (path == nullptr) {
        cache.clear();
    } else {
        cache.erase(path);
    }
}

void BehaviorTreeSystem::Update(Registry& registry, float dt)
{
    if (EngineKernel::Instance().GetMode() != EngineMode::Play) return;

    // v2.0 state-bound model: BT path is resolved from the current StateNode.
    // Falls back to BehaviorTreeAssetComponent.assetPath only when the
    // state has no behaviorTreePath of its own (v1.0 compatibility).
    Signature sig = CreateSignature<
        EnemyTagComponent,
        BehaviorTreeRuntimeComponent,
        BlackboardComponent,
        TransformComponent,
        LocomotionStateComponent,
        ActionStateComponent,
        StateMachineParamsComponent,
        StateMachineAssetComponent>();

    for (auto* arch : registry.GetAllArchetypes()) {
        if (!SignatureMatches(arch->GetSignature(), sig)) continue;

        auto* rtCol     = arch->GetColumn(TypeManager::GetComponentTypeID<BehaviorTreeRuntimeComponent>());
        auto* bbCol     = arch->GetColumn(TypeManager::GetComponentTypeID<BlackboardComponent>());
        auto* trCol     = arch->GetColumn(TypeManager::GetComponentTypeID<TransformComponent>());
        auto* locoCol   = arch->GetColumn(TypeManager::GetComponentTypeID<LocomotionStateComponent>());
        auto* actCol    = arch->GetColumn(TypeManager::GetComponentTypeID<ActionStateComponent>());
        auto* smCol     = arch->GetColumn(TypeManager::GetComponentTypeID<StateMachineParamsComponent>());
        auto* smAssetCol= arch->GetColumn(TypeManager::GetComponentTypeID<StateMachineAssetComponent>());
        if (!rtCol || !bbCol || !trCol || !locoCol || !actCol || !smCol || !smAssetCol) continue;

        const auto& ents = arch->GetEntities();
        for (size_t i = 0; i < arch->GetEntityCount(); ++i) {
            auto& runtime   = *static_cast<BehaviorTreeRuntimeComponent*>(rtCol->Get(i));
            auto& bb        = *static_cast<BlackboardComponent*>(bbCol->Get(i));
            auto& tr        = *static_cast<TransformComponent*>(trCol->Get(i));
            auto& loco      = *static_cast<LocomotionStateComponent*>(locoCol->Get(i));
            auto& act       = *static_cast<ActionStateComponent*>(actCol->Get(i));
            auto& sm        = *static_cast<StateMachineParamsComponent*>(smCol->Get(i));
            auto& smAsset   = *static_cast<StateMachineAssetComponent*>(smAssetCol->Get(i));

            // Reset BT runtime when the SM state changes (per-state BTs
            // must not inherit Wait timers / Cooldown / rising-edge phase
            // from the previous state).
            const uint32_t currentStateId = sm.currentStateId;
            if (currentStateId != runtime.lastTickedStateId) {
                runtime.ResetAll();
                runtime.lastTickedStateId = currentStateId;
            }

            // Resolve which BT to tick for this entity in this state.
            std::string btPath;
            if (const StateNode* stateNode = smAsset.asset.FindState(currentStateId)) {
                if (!stateNode->behaviorTreePath.empty()) btPath = stateNode->behaviorTreePath;
            }
            if (btPath.empty()) {
                // v1.0 fallback: entity-level BT.
                if (auto* legacy = registry.GetComponent<BehaviorTreeAssetComponent>(ents[i])) {
                    if (!legacy->assetPath.empty()) btPath = legacy->assetPath;
                }
            }

            if (btPath.empty()) {
                // No AI configured for this state. SM-only entity.
                continue;
            }

            const BehaviorTreeAsset* asset = LoadOrCache(btPath);
            if (!asset) continue;
            const BTNode* root = asset->FindNode(asset->rootId);
            if (!root) continue;

            // Reset rising-edge trigger params each tick.
            // Attack / DodgeAction handle their own rising-edge phase internally.
            sm.SetParam("Attack", 0.0f);
            sm.SetParam("Dodge", 0.0f);

            BTContext ctx{
                registry,
                ents[i],
                &tr,
                &loco,
                &act,
                &sm,
                registry.GetComponent<HealthComponent>(ents[i]),
                registry.GetComponent<AggroComponent>(ents[i]),
                registry.GetComponent<PerceptionComponent>(ents[i]),
                &bb,
                runtime,
                dt,
            };

            runtime.debugTraceCount = 0;
            (void)Tick(*asset, *root, ctx);
        }
    }
}
