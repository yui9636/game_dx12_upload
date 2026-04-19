#include "StateMachineSystem.h"

#include "Animator/AnimatorComponent.h"
#include "Animator/AnimatorService.h"
#include "Component/ComponentSignature.h"
#include "Component/MeshComponent.h"
#include "Gameplay/HealthComponent.h"
#include "Gameplay/LocomotionStateComponent.h"
#include "Gameplay/PlaybackComponent.h"
#include "Gameplay/StateMachineAssetComponent.h"
#include "Gameplay/StateMachineParamsComponent.h"
#include "Gameplay/StaminaComponent.h"
#include "Gameplay/TimelineAssetRuntimeBuilder.h"
#include "Gameplay/TimelineComponent.h"
#include "Gameplay/TimelineItemBuffer.h"
#include "Gameplay/TimelineLibraryComponent.h"
#include "Input/InputActionMapComponent.h"
#include "Input/ResolvedInputStateComponent.h"
#include "Model/Model.h"
#include "PlayerEditor/StateMachineAsset.h"
#include "Registry/Registry.h"
#include "System/Query.h"

#include <algorithm>
#include <cstdlib>

namespace
{
    int ResolveInputActionIndex(const char* param, const InputActionMapComponent* inputActionMap)
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

        if (!inputActionMap) {
            return -1;
        }

        const auto& actions = inputActionMap->asset.actions;
        for (int i = 0; i < static_cast<int>(actions.size()); ++i) {
            if (actions[i].actionName == param) {
                return i;
            }
        }
        return -1;
    }

    float ResolveStateAnimationClipLength(Registry& registry, EntityID entity, const StateNode& state)
    {
        const MeshComponent* mesh = registry.GetComponent<MeshComponent>(entity);
        if (!mesh || !mesh->model || state.animationIndex < 0) {
            return 0.0f;
        }

        const auto& animations = mesh->model->GetAnimations();
        if (state.animationIndex < 0 || state.animationIndex >= static_cast<int>(animations.size())) {
            return 0.0f;
        }

        return animations[state.animationIndex].secondsLength > 0.0f
            ? animations[state.animationIndex].secondsLength
            : 0.0f;
    }

    const TimelineAsset* FindTimelineAssetById(const TimelineLibraryComponent* timelineLibrary, uint32_t timelineId)
    {
        if (!timelineLibrary || timelineId == 0) {
            return nullptr;
        }

        for (const auto& asset : timelineLibrary->assets) {
            if (asset.id == timelineId) {
                return &asset;
            }
        }
        return nullptr;
    }

    void ResetTimelineRuntime(Registry& registry, EntityID entity)
    {
        if (TimelineComponent* timeline = registry.GetComponent<TimelineComponent>(entity)) {
            *timeline = TimelineComponent{};
        }
        if (TimelineItemBuffer* buffer = registry.GetComponent<TimelineItemBuffer>(entity)) {
            buffer->items.clear();
        }
    }

    void ApplyTimelineStateAsset(
        Registry& registry,
        EntityID entity,
        const StateNode& state,
        const TimelineLibraryComponent* timelineLibrary)
    {
        TimelineItemBuffer* buffer = registry.GetComponent<TimelineItemBuffer>(entity);
        TimelineComponent* timeline = registry.GetComponent<TimelineComponent>(entity);
        if (!buffer || !timeline) {
            return;
        }

        const TimelineAsset* asset = FindTimelineAssetById(timelineLibrary, state.timelineId);
        if (!asset) {
            ResetTimelineRuntime(registry, entity);
            return;
        }

        TimelineAssetRuntimeBuilder::Build(*asset, state.animationIndex, *timeline, *buffer);
    }

    void SyncLocomotionParameters(
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

    bool EvaluateCondition(
        const TransitionCondition& cond,
        const StateMachineParamsComponent& params,
        const InputActionMapComponent* inputActionMap,
        const ResolvedInputStateComponent* input,
        const HealthComponent* health,
        const StaminaComponent* stamina)
    {
        float lhs = 0.0f;

        switch (cond.type) {
        case ConditionType::Input:
            if (input) {
                const int idx = ResolveInputActionIndex(cond.param, inputActionMap);
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
            if (health) {
                lhs = static_cast<float>(health->health);
            }
            break;
        case ConditionType::Stamina:
            if (stamina) {
                lhs = stamina->current;
            }
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
}

void StateMachineSystem::Update(Registry& registry, float dt)
{
    Signature sig = CreateSignature<StateMachineParamsComponent, PlaybackComponent, StateMachineAssetComponent>();

    for (auto* arch : registry.GetAllArchetypes()) {
        if (!SignatureMatches(arch->GetSignature(), sig)) {
            continue;
        }

        auto* smpCol = arch->GetColumn(TypeManager::GetComponentTypeID<StateMachineParamsComponent>());
        auto* pbCol = arch->GetColumn(TypeManager::GetComponentTypeID<PlaybackComponent>());
        auto* assetCol = arch->GetColumn(TypeManager::GetComponentTypeID<StateMachineAssetComponent>());
        if (!smpCol || !pbCol || !assetCol) {
            continue;
        }

        const auto inputId = TypeManager::GetComponentTypeID<ResolvedInputStateComponent>();
        const auto inputMapId = TypeManager::GetComponentTypeID<InputActionMapComponent>();
        const auto healthId = TypeManager::GetComponentTypeID<HealthComponent>();
        const auto locoId = TypeManager::GetComponentTypeID<LocomotionStateComponent>();
        const auto staminaId = TypeManager::GetComponentTypeID<StaminaComponent>();
        const auto timelineLibraryId = TypeManager::GetComponentTypeID<TimelineLibraryComponent>();

        auto* inputCol = arch->GetSignature().test(inputId) ? arch->GetColumn(inputId) : nullptr;
        auto* inputMapCol = arch->GetSignature().test(inputMapId) ? arch->GetColumn(inputMapId) : nullptr;
        auto* healthCol = arch->GetSignature().test(healthId) ? arch->GetColumn(healthId) : nullptr;
        auto* locoCol = arch->GetSignature().test(locoId) ? arch->GetColumn(locoId) : nullptr;
        auto* staminaCol = arch->GetSignature().test(staminaId) ? arch->GetColumn(staminaId) : nullptr;
        auto* timelineLibraryCol = arch->GetSignature().test(timelineLibraryId) ? arch->GetColumn(timelineLibraryId) : nullptr;

        for (size_t i = 0; i < arch->GetEntityCount(); ++i) {
            const EntityID entity = arch->GetEntities()[i];
            auto& smp = *static_cast<StateMachineParamsComponent*>(smpCol->Get(i));
            auto& pb = *static_cast<PlaybackComponent*>(pbCol->Get(i));
            const auto& stateMachineAssetComponent = *static_cast<StateMachineAssetComponent*>(assetCol->Get(i));

            auto* input = inputCol ? static_cast<ResolvedInputStateComponent*>(inputCol->Get(i)) : nullptr;
            auto* inputActionMap = inputMapCol ? static_cast<InputActionMapComponent*>(inputMapCol->Get(i)) : nullptr;
            auto* health = healthCol ? static_cast<HealthComponent*>(healthCol->Get(i)) : nullptr;
            auto* loco = locoCol ? static_cast<LocomotionStateComponent*>(locoCol->Get(i)) : nullptr;
            auto* stamina = staminaCol ? static_cast<StaminaComponent*>(staminaCol->Get(i)) : nullptr;
            auto* timelineLibrary = timelineLibraryCol ? static_cast<TimelineLibraryComponent*>(timelineLibraryCol->Get(i)) : nullptr;

            const StateMachineAsset& asset = stateMachineAssetComponent.asset;
            if (asset.states.empty()) {
                continue;
            }

            SyncLocomotionParameters(smp, loco);

            if (smp.currentStateId == 0) {
                smp.currentStateId = asset.defaultStateId;
                smp.stateTimer = 0.0f;

                if (const StateNode* initialState = asset.FindState(smp.currentStateId)) {
                    AnimatorService::Instance().PlayBase(
                        entity,
                        initialState->animationIndex,
                        initialState->loopAnimation,
                        0.0f,
                        initialState->animSpeed);
                    pb.currentSeconds = 0.0f;
                    pb.clipLength = ResolveStateAnimationClipLength(registry, entity, *initialState);
                    pb.playing = true;
                    pb.looping = initialState->loopAnimation;
                    pb.playSpeed = initialState->animSpeed;
                    pb.finished = false;
                    ApplyTimelineStateAsset(registry, entity, *initialState, timelineLibrary);
                }
            }

            smp.stateTimer += dt;
            smp.animFinished = pb.finished;

            auto transitions = asset.GetTransitionsFrom(smp.currentStateId);
            const StateTransition* best = nullptr;
            int bestPriority = -1;

            for (const StateTransition* transition : transitions) {
                if (transition->hasExitTime) {
                    float normalizedTime = 1.0f;
                    if (pb.clipLength > 0.0f) {
                        normalizedTime = pb.currentSeconds / pb.clipLength;
                    }
                    if (normalizedTime < transition->exitTimeNormalized) {
                        continue;
                    }
                }

                bool allConditionsMet = true;
                for (const auto& condition : transition->conditions) {
                    if (!EvaluateCondition(condition, smp, inputActionMap, input, health, stamina)) {
                        allConditionsMet = false;
                        break;
                    }
                }

                if (allConditionsMet && transition->priority > bestPriority) {
                    best = transition;
                    bestPriority = transition->priority;
                }
            }

            if (!best) {
                continue;
            }

            smp.currentStateId = best->toState;
            smp.stateTimer = 0.0f;
            smp.animFinished = false;

            const StateNode* newState = asset.FindState(best->toState);
            if (!newState) {
                continue;
            }

            pb.currentSeconds = 0.0f;
            pb.clipLength = ResolveStateAnimationClipLength(registry, entity, *newState);
            pb.playing = true;
            pb.looping = newState->loopAnimation;
            pb.playSpeed = newState->animSpeed;
            pb.finished = false;

            AnimatorService::Instance().PlayBase(
                entity,
                newState->animationIndex,
                newState->loopAnimation,
                best->blendDuration,
                newState->animSpeed);
            ApplyTimelineStateAsset(registry, entity, *newState, timelineLibrary);
        }
    }
}

void StateMachineSystem::InvalidateAssetCache(const char* /*path*/)
{
    TimelineAssetRuntimeBuilder::InvalidateAssetCache();
}
