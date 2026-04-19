#include "PlayerRuntimeSetup.h"

#include "Component/NodeSocketComponent.h"
#include "Gameplay/ActionDatabaseComponent.h"
#include "Gameplay/ActionStateComponent.h"
#include "Gameplay/AnimatorComponent.h"
#include "Gameplay/CharacterPhysicsComponent.h"
#include "Gameplay/DodgeStateComponent.h"
#include "Gameplay/HealthComponent.h"
#include "Gameplay/HitboxTrackingComponent.h"
#include "Gameplay/LocomotionStateComponent.h"
#include "Gameplay/PlaybackComponent.h"
#include "Gameplay/PlayerTagComponent.h"
#include "Gameplay/StateMachineParamsComponent.h"
#include "Gameplay/StaminaComponent.h"
#include "Gameplay/TimelineComponent.h"
#include "Gameplay/TimelineItemBuffer.h"
#include "Input/InputBindingComponent.h"
#include "Input/InputContextComponent.h"
#include "Input/InputUserComponent.h"
#include "Input/ResolvedInputStateComponent.h"
#include "Registry/Registry.h"

namespace
{
    template<typename T>
    T* EnsureComponent(Registry& registry, EntityID entity)
    {
        if (auto* component = registry.GetComponent<T>(entity)) {
            return component;
        }
        registry.AddComponent(entity, T{});
        return registry.GetComponent<T>(entity);
    }

    void ResetLocomotionStateMachineParams(StateMachineParamsComponent& stateMachine)
    {
        stateMachine.SetParam("MoveX", 0.0f);
        stateMachine.SetParam("MoveY", 0.0f);
        stateMachine.SetParam("MoveMagnitude", 0.0f);
        stateMachine.SetParam("IsMoving", 0.0f);
    }
}

namespace PlayerRuntimeSetup
{
    void EnsurePlayerPersistentComponents(Registry& registry, EntityID entity)
    {
        if (Entity::IsNull(entity) || !registry.IsAlive(entity)) {
            return;
        }

        PlayerTagComponent* playerTag = EnsureComponent<PlayerTagComponent>(registry, entity);
        if (playerTag && playerTag->playerId == 0) {
            playerTag->playerId = 1;
        }

        EnsureComponent<CharacterPhysicsComponent>(registry, entity);
        EnsureComponent<HealthComponent>(registry, entity);
        EnsureComponent<StaminaComponent>(registry, entity);
        EnsureComponent<ActionDatabaseComponent>(registry, entity);
        EnsureComponent<InputBindingComponent>(registry, entity);
        EnsureComponent<NodeSocketComponent>(registry, entity);
        EnsureComponent<StateMachineParamsComponent>(registry, entity);

        InputContextComponent* inputContext = EnsureComponent<InputContextComponent>(registry, entity);
        if (inputContext) {
            inputContext->priority = InputContextPriority::RuntimeGameplay;
            inputContext->consumed = false;
        }

        InputUserComponent* inputUser = EnsureComponent<InputUserComponent>(registry, entity);
        if (inputUser) {
            inputUser->userId = (playerTag && playerTag->playerId != 0) ? playerTag->playerId : 1;
            inputUser->isPrimary = (inputUser->userId == 1);
            inputUser->isEditorUser = false;
        }
    }

    void EnsurePlayerRuntimeComponents(Registry& registry, EntityID entity)
    {
        if (Entity::IsNull(entity) || !registry.IsAlive(entity)) {
            return;
        }

        EnsureComponent<ResolvedInputStateComponent>(registry, entity);
        EnsureComponent<PlaybackComponent>(registry, entity);
        EnsureComponent<TimelineComponent>(registry, entity);
        EnsureComponent<TimelineItemBuffer>(registry, entity);
        EnsureComponent<HitboxTrackingComponent>(registry, entity);
        EnsureComponent<LocomotionStateComponent>(registry, entity);
        EnsureComponent<ActionStateComponent>(registry, entity);
        EnsureComponent<DodgeStateComponent>(registry, entity);
        EnsureComponent<AnimatorComponent>(registry, entity);
    }

    void ResetPlayerRuntimeState(Registry& registry, EntityID entity)
    {
        if (Entity::IsNull(entity) || !registry.IsAlive(entity)) {
            return;
        }

        if (auto* stateMachine = registry.GetComponent<StateMachineParamsComponent>(entity)) {
            stateMachine->currentStateId = 0;
            stateMachine->stateTimer = 0.0f;
            stateMachine->animFinished = false;
            ResetLocomotionStateMachineParams(*stateMachine);
        }

        if (auto* inputContext = registry.GetComponent<InputContextComponent>(entity)) {
            inputContext->consumed = false;
        }

        if (auto* resolved = registry.GetComponent<ResolvedInputStateComponent>(entity)) {
            *resolved = ResolvedInputStateComponent{};
        }

        if (auto* playback = registry.GetComponent<PlaybackComponent>(entity)) {
            *playback = PlaybackComponent{};
        }

        if (auto* timeline = registry.GetComponent<TimelineComponent>(entity)) {
            *timeline = TimelineComponent{};
        }

        if (auto* timelineItems = registry.GetComponent<TimelineItemBuffer>(entity)) {
            timelineItems->items.clear();
        }

        if (auto* hitboxTracking = registry.GetComponent<HitboxTrackingComponent>(entity)) {
            hitboxTracking->ClearHitList();
        }

        if (auto* locomotion = registry.GetComponent<LocomotionStateComponent>(entity)) {
            locomotion->moveInput = { 0.0f, 0.0f };
            locomotion->inputStrength = 0.0f;
            locomotion->worldMoveDir = { 0.0f, 0.0f };
            locomotion->gaitIndex = 0;
            locomotion->currentSpeed = 0.0f;
            locomotion->targetAngleY = 0.0f;
            locomotion->turningInPlace = false;
            locomotion->lastTurnSign = 0;
        }

        if (auto* action = registry.GetComponent<ActionStateComponent>(entity)) {
            *action = ActionStateComponent{};
        }

        if (auto* dodge = registry.GetComponent<DodgeStateComponent>(entity)) {
            dodge->dodgeTimer = 0.0f;
            dodge->dodgeAngleY = 0.0f;
            dodge->dodgeTriggered = false;
        }

        if (auto* physics = registry.GetComponent<CharacterPhysicsComponent>(entity)) {
            physics->velocity = { 0.0f, 0.0f, 0.0f };
            physics->verticalVelocity = 0.0f;
            physics->isGround = true;
        }

        if (auto* animator = registry.GetComponent<AnimatorComponent>(entity)) {
            *animator = AnimatorComponent{};
        }
    }

    bool HasMinimumPlayerAuthoringComponents(Registry& registry, EntityID entity)
    {
        if (Entity::IsNull(entity) || !registry.IsAlive(entity)) {
            return false;
        }

        return registry.GetComponent<PlayerTagComponent>(entity) != nullptr
            || registry.GetComponent<StateMachineParamsComponent>(entity) != nullptr
            || registry.GetComponent<InputBindingComponent>(entity) != nullptr
            || registry.GetComponent<InputUserComponent>(entity) != nullptr;
    }
}
