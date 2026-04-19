#include "StateMachineSystem.h"
#include "StateMachineParamsComponent.h"
#include "PlayerEditor/StateMachineAsset.h"
#include "PlayerEditor/StateMachineAssetSerializer.h"
#include "Input/InputActionMapAsset.h"
#include "Input/InputBindingComponent.h"
#include "Input/ResolvedInputStateComponent.h"
#include "Gameplay/HealthComponent.h"
#include "Gameplay/LocomotionStateComponent.h"
#include "Gameplay/StaminaComponent.h"
#include "Gameplay/PlaybackComponent.h"
#include "Gameplay/TimelineAssetRuntimeBuilder.h"
#include "Gameplay/TimelineComponent.h"
#include "Gameplay/TimelineItemBuffer.h"
#include "Animator/AnimatorService.h"
#include "Animator/AnimatorComponent.h"
#include "Component/MeshComponent.h"
#include "Model/Model.h"
#include "Registry/Registry.h"
#include "Component/ComponentSignature.h"
#include "System/Query.h"

#include <cstdlib>
#include <string>
#include <unordered_map>
#include <algorithm>

// ============================================================================
// Cached assets
// ============================================================================

static std::unordered_map<std::string, StateMachineAsset> s_assetCache;

static const StateMachineAsset* GetCachedAsset(const char* path)
{
    if (!path || path[0] == '\0') return nullptr;
    std::string key(path);
    auto it = s_assetCache.find(key);
    if (it != s_assetCache.end()) return &it->second;

    StateMachineAsset asset;
    if (StateMachineAssetSerializer::Load(key, asset)) {
        s_assetCache[key] = std::move(asset);
        return &s_assetCache[key];
    }
    return nullptr;
}

static int ResolveInputActionIndex(const char* param, const InputBindingComponent* binding)
{
    if (!param || param[0] == '\0') {
        return -1;
    }

    bool numeric = true;
    for (const char* p = param; *p != '\0'; ++p) {
        if (*p < '0' || *p > '9') {
            numeric = false;
            break;
        }
    }
    if (numeric) {
        return atoi(param);
    }

    if (!binding || binding->actionMapAssetPath[0] == '\0') {
        return -1;
    }

    InputActionMapAsset* actionMap = InputActionMapAsset::Get(binding->actionMapAssetPath);
    if (!actionMap) {
        return -1;
    }

    for (int i = 0; i < static_cast<int>(actionMap->actions.size()); ++i) {
        if (actionMap->actions[i].actionName == param) {
            return i;
        }
    }
    return -1;
}

static float ResolveStateAnimationClipLength(Registry& registry, EntityID entity, const StateNode& state)
{
    const MeshComponent* mesh = registry.GetComponent<MeshComponent>(entity);
    if (!mesh || !mesh->model || state.animationIndex < 0) {
        return 0.0f;
    }

    const auto& animations = mesh->model->GetAnimations();
    if (state.animationIndex < 0 || state.animationIndex >= static_cast<int>(animations.size())) {
        return 0.0f;
    }

    return (std::max)(0.0f, animations[state.animationIndex].secondsLength);
}

static void ApplyTimelineStateAsset(Registry& registry, EntityID entity, const StateNode& state)
{
    TimelineItemBuffer* buffer = registry.GetComponent<TimelineItemBuffer>(entity);
    TimelineComponent* timeline = registry.GetComponent<TimelineComponent>(entity);
    if (!buffer || !timeline) {
        return;
    }

    TimelineAssetRuntimeBuilder::BuildFromPath(state.timelineAssetPath, state.animationIndex, *timeline, *buffer);
}

static void SyncLocomotionParameters(
    StateMachineParamsComponent& params,
    const LocomotionStateComponent* locomotion)
{
    if (!locomotion) {
        return;
    }

    params.SetParam("MoveX", locomotion->moveInput.x);
    params.SetParam("MoveY", locomotion->moveInput.y);
    params.SetParam("MoveMagnitude", locomotion->inputStrength);
    params.SetParam("IsMoving", locomotion->gaitIndex > 0 ? 1.0f : 0.0f);
}

// ============================================================================
// Condition evaluation
// ============================================================================

static bool EvaluateCondition(const TransitionCondition& cond,
    const StateMachineParamsComponent& params,
    const InputBindingComponent* binding,
    const ResolvedInputStateComponent* input,
    const HealthComponent* health,
    const StaminaComponent* stamina)
{
    float lhs = 0.0f;

    switch (cond.type) {
    case ConditionType::Input:
        if (input) {
            int idx = ResolveInputActionIndex(cond.param, binding);
            if (idx >= 0 && idx < ResolvedInputStateComponent::MAX_ACTIONS) {
                lhs = input->actions[idx].pressed ? 1.0f : 0.0f;
            }
        }
        break;
    case ConditionType::Timer:
        lhs = params.stateTimer;
        break;
    case ConditionType::AnimEnd:
        lhs = params.animFinished ? 1.0f : 0.0f;
        break;
    case ConditionType::Health:
        if (health) lhs = (float)health->health;
        break;
    case ConditionType::Stamina:
        if (stamina) lhs = stamina->current;
        break;
    case ConditionType::Parameter:
        lhs = params.GetParam(cond.param);
        break;
    }

    switch (cond.compare) {
    case CompareOp::Equal:        return lhs == cond.value;
    case CompareOp::NotEqual:     return lhs != cond.value;
    case CompareOp::Greater:      return lhs > cond.value;
    case CompareOp::Less:         return lhs < cond.value;
    case CompareOp::GreaterEqual: return lhs >= cond.value;
    case CompareOp::LessEqual:    return lhs <= cond.value;
    }
    return false;
}

// ============================================================================
// System Update
// ============================================================================

void StateMachineSystem::Update(Registry& registry, float dt)
{
    Signature sig = CreateSignature<StateMachineParamsComponent, PlaybackComponent>();

    for (auto* arch : registry.GetAllArchetypes()) {
        if (!SignatureMatches(arch->GetSignature(), sig)) continue;

        auto* smpCol = arch->GetColumn(TypeManager::GetComponentTypeID<StateMachineParamsComponent>());
        auto* pbCol  = arch->GetColumn(TypeManager::GetComponentTypeID<PlaybackComponent>());
        if (!smpCol || !pbCol) continue;

        // Optional columns
        auto inputId   = TypeManager::GetComponentTypeID<ResolvedInputStateComponent>();
        auto bindingId = TypeManager::GetComponentTypeID<InputBindingComponent>();
        auto healthId  = TypeManager::GetComponentTypeID<HealthComponent>();
        auto locoId    = TypeManager::GetComponentTypeID<LocomotionStateComponent>();
        auto staminaId = TypeManager::GetComponentTypeID<StaminaComponent>();

        auto* inputCol   = arch->GetSignature().test(inputId)   ? arch->GetColumn(inputId)   : nullptr;
        auto* bindingCol = arch->GetSignature().test(bindingId) ? arch->GetColumn(bindingId) : nullptr;
        auto* healthCol  = arch->GetSignature().test(healthId)  ? arch->GetColumn(healthId)  : nullptr;
        auto* locoCol    = arch->GetSignature().test(locoId)    ? arch->GetColumn(locoId)    : nullptr;
        auto* staminaCol = arch->GetSignature().test(staminaId) ? arch->GetColumn(staminaId) : nullptr;

        for (size_t i = 0; i < arch->GetEntityCount(); ++i) {
            const EntityID entity = arch->GetEntities()[i];
            auto& smp = *static_cast<StateMachineParamsComponent*>(smpCol->Get(i));
            auto& pb  = *static_cast<PlaybackComponent*>(pbCol->Get(i));

            auto* input   = inputCol   ? static_cast<ResolvedInputStateComponent*>(inputCol->Get(i))   : nullptr;
            auto* binding = bindingCol ? static_cast<InputBindingComponent*>(bindingCol->Get(i)) : nullptr;
            auto* health  = healthCol  ? static_cast<HealthComponent*>(healthCol->Get(i))  : nullptr;
            auto* loco    = locoCol    ? static_cast<LocomotionStateComponent*>(locoCol->Get(i)) : nullptr;
            auto* stamina = staminaCol ? static_cast<StaminaComponent*>(staminaCol->Get(i)) : nullptr;

            const StateMachineAsset* asset = GetCachedAsset(smp.assetPath);
            if (!asset) continue;

            SyncLocomotionParameters(smp, loco);

            // Initialize
            if (smp.currentStateId == 0) {
                smp.currentStateId = asset->defaultStateId;
                smp.stateTimer = 0.0f;
                if (const StateNode* initialState = asset->FindState(smp.currentStateId)) {
                    AnimatorService::Instance().PlayBase(entity, initialState->animationIndex, initialState->loopAnimation, 0.0f, initialState->animSpeed);
                    pb.currentSeconds = 0.0f;
                    pb.clipLength = ResolveStateAnimationClipLength(registry, entity, *initialState);
                    pb.playing = true;
                    pb.looping = initialState->loopAnimation;
                    pb.playSpeed = initialState->animSpeed;
                    pb.finished = false;
                    ApplyTimelineStateAsset(registry, entity, *initialState);
                }
            }

            smp.stateTimer += dt;
            smp.animFinished = pb.finished;

            // Evaluate transitions
            auto transitions = asset->GetTransitionsFrom(smp.currentStateId);
            const StateTransition* best = nullptr;
            int bestPri = -1;

            for (auto* trans : transitions) {
                if (trans->hasExitTime) {
                    float norm = (pb.clipLength > 0) ? pb.currentSeconds / pb.clipLength : 1.0f;
                    if (norm < trans->exitTimeNormalized) continue;
                }

                bool allMet = true;
                for (auto& cond : trans->conditions) {
                    if (!EvaluateCondition(cond, smp, binding, input, health, stamina)) {
                        allMet = false;
                        break;
                    }
                }

                if (allMet && trans->priority > bestPri) {
                    best = trans;
                    bestPri = trans->priority;
                }
            }

            if (best) {
                smp.currentStateId = best->toState;
                smp.stateTimer = 0.0f;
                smp.animFinished = false;

                const StateNode* newState = asset->FindState(best->toState);
                if (newState) {
                    pb.currentSeconds = 0.0f;
                    pb.clipLength = ResolveStateAnimationClipLength(registry, entity, *newState);
                    pb.playing = true;
                    pb.looping = newState->loopAnimation;
                    pb.playSpeed = newState->animSpeed;
                    pb.finished = false;
                    AnimatorService::Instance().PlayBase(entity, newState->animationIndex, newState->loopAnimation, best->blendDuration, newState->animSpeed);
                    ApplyTimelineStateAsset(registry, entity, *newState);
                }
            }
        }
    }
}

void StateMachineSystem::InvalidateAssetCache(const char* path)
{
    if (!path || path[0] == '\0') {
        s_assetCache.clear();
        TimelineAssetRuntimeBuilder::InvalidateAssetCache();
        return;
    }

    s_assetCache.erase(path);
    TimelineAssetRuntimeBuilder::InvalidateAssetCache(path);
}
