#include "StateMachineSystem.h"
#include "StateMachineParamsComponent.h"
#include "PlayerEditor/StateMachineAsset.h"
#include "PlayerEditor/StateMachineAssetSerializer.h"
#include "Input/ResolvedInputStateComponent.h"
#include "Gameplay/HealthComponent.h"
#include "Gameplay/StaminaComponent.h"
#include "Gameplay/PlaybackComponent.h"
#include "Animator/AnimatorService.h"
#include "Animator/AnimatorComponent.h"
#include "Registry/Registry.h"
#include "Component/ComponentSignature.h"
#include "System/Query.h"

#include <string>
#include <unordered_map>
#include <cstring>

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

// ============================================================================
// Condition evaluation
// ============================================================================

static bool EvaluateCondition(const TransitionCondition& cond,
    const StateMachineParamsComponent& params,
    const ResolvedInputStateComponent* input,
    const HealthComponent* health,
    const StaminaComponent* stamina)
{
    float lhs = 0.0f;

    switch (cond.type) {
    case ConditionType::Input:
        if (input) {
            int idx = atoi(cond.param);
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
        auto healthId  = TypeManager::GetComponentTypeID<HealthComponent>();
        auto staminaId = TypeManager::GetComponentTypeID<StaminaComponent>();

        auto* inputCol   = arch->GetSignature().test(inputId)   ? arch->GetColumn(inputId)   : nullptr;
        auto* healthCol  = arch->GetSignature().test(healthId)  ? arch->GetColumn(healthId)  : nullptr;
        auto* staminaCol = arch->GetSignature().test(staminaId) ? arch->GetColumn(staminaId) : nullptr;

        for (size_t i = 0; i < arch->GetEntityCount(); ++i) {
            const EntityID entity = arch->GetEntities()[i];
            auto& smp = *static_cast<StateMachineParamsComponent*>(smpCol->Get(i));
            auto& pb  = *static_cast<PlaybackComponent*>(pbCol->Get(i));

            auto* input   = inputCol   ? static_cast<ResolvedInputStateComponent*>(inputCol->Get(i))   : nullptr;
            auto* health  = healthCol  ? static_cast<HealthComponent*>(healthCol->Get(i))  : nullptr;
            auto* stamina = staminaCol ? static_cast<StaminaComponent*>(staminaCol->Get(i)) : nullptr;

            const StateMachineAsset* asset = GetCachedAsset(smp.assetPath);
            if (!asset) continue;

            // Initialize
            if (smp.currentStateId == 0) {
                smp.currentStateId = asset->defaultStateId;
                smp.stateTimer = 0.0f;
                if (const StateNode* initialState = asset->FindState(smp.currentStateId)) {
                    if (registry.GetComponent<AnimatorComponent>(entity)) {
                        AnimatorService::Instance().PlayBase(entity, initialState->animationIndex, initialState->loopAnimation, 0.0f, initialState->animSpeed);
                    }
                    pb.currentSeconds = 0.0f;
                    pb.playing = true;
                    pb.looping = initialState->loopAnimation;
                    pb.playSpeed = initialState->animSpeed;
                    pb.finished = false;
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
                    if (!EvaluateCondition(cond, smp, input, health, stamina)) {
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
                    pb.playing = true;
                    pb.looping = newState->loopAnimation;
                    pb.playSpeed = newState->animSpeed;
                    pb.finished = false;
                    if (registry.GetComponent<AnimatorComponent>(entity)) {
                        AnimatorService::Instance().PlayBase(entity, newState->animationIndex, newState->loopAnimation, best->blendDuration, newState->animSpeed);
                    }
                }
            }
        }
    }
}
