#include "PlayerRuntimeSetup.h"

#include "Component/ColliderComponent.h"
#include "Component/MeshComponent.h"
#include "Component/NodeSocketComponent.h"
#include "Collision/CollisionManager.h"
#include "Gameplay/ActionDatabaseComponent.h"
#include "Gameplay/ActionStateComponent.h"
#include "Gameplay/AnimatorComponent.h"
#include "Gameplay/CharacterPhysicsComponent.h"
#include "Gameplay/DodgeStateComponent.h"
#include "Gameplay/HealthComponent.h"
#include "Gameplay/HitStopComponent.h"
#include "Gameplay/HitboxTrackingComponent.h"
#include "Gameplay/LocomotionStateComponent.h"
#include "Gameplay/PlaybackComponent.h"
#include "Gameplay/StateMachineAssetComponent.h"
#include "Gameplay/PlayerTagComponent.h"
#include "Component/ActorTypeComponent.h"
#include "Gameplay/StateMachineParamsComponent.h"
#include "Gameplay/StaminaComponent.h"
#include "Gameplay/TimelineLibraryComponent.h"
#include "Gameplay/TimelineComponent.h"
#include "Gameplay/TimelineItemBuffer.h"
#include "Input/InputActionMapComponent.h"
#include "Input/InputBindingComponent.h"
#include "Input/InputContextComponent.h"
#include "Input/InputUserComponent.h"
#include "Input/ResolvedInputStateComponent.h"
#include "Model/Model.h"
#include "Registry/Registry.h"
#include "System/Query.h"

#include <algorithm>
#include <cctype>
#include <string>
#include <vector>

namespace
{
    constexpr uint32_t kScancodeA = 4;
    constexpr uint32_t kScancodeD = 7;
    constexpr uint32_t kScancodeS = 22;
    constexpr uint32_t kScancodeW = 26;
    constexpr uint32_t kScancodeJ = 13;
    constexpr uint32_t kScancodeSpace = 44;
    constexpr uint8_t kMouseButtonLeft = 1;
    constexpr uint8_t kGamepadButtonX = 2;
    constexpr uint8_t kGamepadButtonB = 1;
    constexpr uint8_t kGamepadAxisLeftX = 0;
    constexpr uint8_t kGamepadAxisLeftY = 1;

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
        stateMachine.SetParam("Gait", 0.0f);
        stateMachine.SetParam("IsWalking", 0.0f);
        stateMachine.SetParam("IsRunning", 0.0f);
        stateMachine.SetParam("Attack", 0.0f);
        stateMachine.SetParam("Dodge", 0.0f);
        stateMachine.SetParam("Damaged", 0.0f);
    }

    AxisBinding* FindAxisBinding(InputActionMapAsset& map, const char* axisName)
    {
        for (auto& axis : map.axes) {
            if (axis.axisName == axisName) {
                return &axis;
            }
        }
        return nullptr;
    }

    ActionBinding* FindActionBinding(InputActionMapAsset& map, const char* actionName)
    {
        for (auto& action : map.actions) {
            if (action.actionName == actionName) {
                return &action;
            }
        }
        return nullptr;
    }

    void EnsureActionBinding(
        InputActionMapAsset& map,
        const char* actionName,
        uint32_t scancode,
        uint8_t mouseButton,
        uint8_t gamepadButton)
    {
        ActionBinding* action = FindActionBinding(map, actionName);
        if (!action) {
            ActionBinding newAction;
            newAction.actionName = actionName;
            map.actions.push_back(newAction);
            action = &map.actions.back();
        }

        if (action->scancode == 0) {
            action->scancode = scancode;
        }
        if (action->mouseButton == 0) {
            action->mouseButton = mouseButton;
        }
        if (action->gamepadButton == 0xFF) {
            action->gamepadButton = gamepadButton;
        }
        action->trigger = ActionTriggerType::Pressed;
    }

    void EnsureAxisBinding(
        InputActionMapAsset& map,
        const char* axisName,
        uint32_t positiveKey,
        uint32_t negativeKey,
        uint8_t gamepadAxis)
    {
        AxisBinding* axis = FindAxisBinding(map, axisName);
        if (!axis) {
            AxisBinding newAxis;
            newAxis.axisName = axisName;
            map.axes.push_back(newAxis);
            axis = &map.axes.back();
        }

        if (axis->positiveKey == 0) {
            axis->positiveKey = positiveKey;
        }
        if (axis->negativeKey == 0) {
            axis->negativeKey = negativeKey;
        }
        if (axis->gamepadAxis == 0xFF) {
            axis->gamepadAxis = gamepadAxis;
        }
        if (axis->deadzone <= 0.0f) {
            axis->deadzone = 0.15f;
        }
        if (axis->sensitivity == 0.0f) {
            axis->sensitivity = 1.0f;
        }
    }

    void MoveAxisBindingTo(InputActionMapAsset& map, const char* axisName, size_t targetIndex)
    {
        for (size_t i = 0; i < map.axes.size(); ++i) {
            if (map.axes[i].axisName != axisName) {
                continue;
            }
            if (i == targetIndex) {
                return;
            }

            AxisBinding axis = map.axes[i];
            map.axes.erase(map.axes.begin() + static_cast<std::vector<AxisBinding>::difference_type>(i));
            if (targetIndex > map.axes.size()) {
                targetIndex = map.axes.size();
            }
            map.axes.insert(map.axes.begin() + static_cast<std::vector<AxisBinding>::difference_type>(targetIndex), axis);
            return;
        }
    }

    void MoveActionBindingTo(InputActionMapAsset& map, const char* actionName, size_t targetIndex)
    {
        for (size_t i = 0; i < map.actions.size(); ++i) {
            if (map.actions[i].actionName != actionName) {
                continue;
            }
            if (i == targetIndex) {
                return;
            }

            ActionBinding action = map.actions[i];
            map.actions.erase(map.actions.begin() + static_cast<std::vector<ActionBinding>::difference_type>(i));
            if (targetIndex > map.actions.size()) {
                targetIndex = map.actions.size();
            }
            map.actions.insert(map.actions.begin() + static_cast<std::vector<ActionBinding>::difference_type>(targetIndex), action);
            return;
        }
    }

    void EnsureDefaultPlayerInputMap(InputActionMapAsset& map)
    {
        if (map.name.empty()) {
            map.name = "PlayerDefault";
        }
        if (map.contextCategory.empty()) {
            map.contextCategory = "RuntimeGameplay";
        }

        EnsureAxisBinding(map, "MoveX", kScancodeD, kScancodeA, kGamepadAxisLeftX);
        EnsureAxisBinding(map, "MoveY", kScancodeW, kScancodeS, kGamepadAxisLeftY);
        EnsureActionBinding(map, "Attack", kScancodeJ, kMouseButtonLeft, kGamepadButtonX);
        EnsureActionBinding(map, "Dodge", kScancodeSpace, 0, kGamepadButtonB);
        MoveActionBindingTo(map, "Attack", 0);
        MoveActionBindingTo(map, "Dodge", 1);
        MoveAxisBindingTo(map, "MoveX", 0);
        MoveAxisBindingTo(map, "MoveY", 1);
    }

    void EnsureLocomotionRuntimeTuning(LocomotionStateComponent& locomotion)
    {
        if (locomotion.walkMaxSpeed > 20.0f ||
            locomotion.jogMaxSpeed > 20.0f ||
            locomotion.runMaxSpeed > 40.0f)
        {
            locomotion.walkMaxSpeed = 1.6f;
            locomotion.jogMaxSpeed = 3.2f;
            locomotion.runMaxSpeed = 5.8f;
        }

        if (locomotion.acceleration > 50.0f) {
            locomotion.acceleration = 12.0f;
        }
        if (locomotion.deceleration > 100.0f) {
            locomotion.deceleration = 18.0f;
        }
        if (locomotion.launchBoost > 2.0f) {
            locomotion.launchBoost = 1.0f;
        }
        if (locomotion.turnSpeed > 1080.0f || locomotion.turnSpeed <= 0.0f) {
            locomotion.turnSpeed = 720.0f;
        }
    }

    std::string ToLowerAscii(std::string value)
    {
        std::transform(
            value.begin(),
            value.end(),
            value.begin(),
            [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        return value;
    }

    // Spec §12: per-slot attack animation lookup (Attack1〜3).
    int FindAttackAnimationBySlot(Registry& registry, EntityID entity, int slot)
    {
        const MeshComponent* mesh = registry.GetComponent<MeshComponent>(entity);
        if (!mesh || !mesh->model) {
            return -1;
        }

        char k1[32], k2[32], k3[32];
        snprintf(k1, sizeof(k1), "combo%d",  slot);
        snprintf(k2, sizeof(k2), "combo_%d", slot);
        snprintf(k3, sizeof(k3), "attack%d", slot);

        const auto& animations = mesh->model->GetAnimations();
        for (int i = 0; i < static_cast<int>(animations.size()); ++i) {
            const std::string loweredName = ToLowerAscii(animations[i].name);
            if (loweredName.find(k1) != std::string::npos ||
                loweredName.find(k2) != std::string::npos ||
                loweredName.find(k3) != std::string::npos) {
                return i;
            }
        }
        return -1;
    }

    // Spec §11: Attack1〜3 nodes with comboStart / cancelStart / damage tuning.
    // Spec §3.3: Idempotent — preserve user-edited fields when re-running.
    void EnsureDefaultActionDatabase(Registry& registry, EntityID entity, ActionDatabaseComponent& database)
    {
        struct AttackTuning {
            float comboStart;
            float cancelStart;
            int damage;
        };
        static const AttackTuning kTuning[3] = {
            { 0.4f, 0.2f, 10 },
            { 0.4f, 0.2f, 12 },
            { 0.5f, 0.3f, 18 },
        };

        const bool needsExpansion = database.nodeCount < 3;
        if (needsExpansion) {
            database.nodeCount = 3;
        }

        for (int slot = 0; slot < 3; ++slot) {
            ActionNode& node = database.nodes[slot];
            const bool wasFresh = needsExpansion && (node.animIndex == 0 && node.damageVal == 0);

            if (node.animIndex < 0 || wasFresh) {
                const int preferred = FindAttackAnimationBySlot(registry, entity, slot + 1);
                if (preferred >= 0) {
                    node.animIndex = preferred;
                } else if (node.animIndex < 0) {
                    node.animIndex = 0;
                }
            }

            if (wasFresh) {
                node.nextLight   = -1;
                node.nextHeavy   = -1;
                node.inputStart  = 0.0f;
                node.inputEnd    = 1.0f;
                node.comboStart  = kTuning[slot].comboStart;
                node.cancelStart = kTuning[slot].cancelStart;
                node.damageVal   = kTuning[slot].damage;
            }
            if (node.animSpeed <= 0.0f) {
                node.animSpeed = 1.0f;
            }
        }
    }

    void AddUniqueEntity(std::vector<EntityID>& entities, EntityID entity)
    {
        if (Entity::IsNull(entity)) {
            return;
        }

        for (EntityID existing : entities) {
            if (existing == entity) {
                return;
            }
        }
        entities.push_back(entity);
    }

    template<typename T>
    void CollectEntitiesWithComponent(Registry& registry, std::vector<EntityID>& entities)
    {
        Query<T> query(registry);
        query.ForEachWithEntity([&](EntityID entity, T&) {
            AddUniqueEntity(entities, entity);
        });
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

        ActorTypeComponent* actorType = EnsureComponent<ActorTypeComponent>(registry, entity);
        if (actorType && actorType->type == ActorType::None) {
            actorType->type = ActorType::Player;
        }

        EnsureComponent<CharacterPhysicsComponent>(registry, entity);
        EnsureComponent<HealthComponent>(registry, entity);
        EnsureComponent<StaminaComponent>(registry, entity);
        ActionDatabaseComponent* actionDatabase = EnsureComponent<ActionDatabaseComponent>(registry, entity);
        if (actionDatabase) {
            EnsureDefaultActionDatabase(registry, entity, *actionDatabase);
        }
        EnsureComponent<StateMachineAssetComponent>(registry, entity);
        EnsureComponent<TimelineLibraryComponent>(registry, entity);
        InputActionMapComponent* inputActionMap = EnsureComponent<InputActionMapComponent>(registry, entity);
        if (inputActionMap) {
            EnsureDefaultPlayerInputMap(inputActionMap->asset);
        }
        EnsureComponent<InputBindingComponent>(registry, entity);
        EnsureComponent<NodeSocketComponent>(registry, entity);
        EnsureComponent<ColliderComponent>(registry, entity);
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
        EnsureComponent<ColliderComponent>(registry, entity);
        EnsureComponent<HitStopComponent>(registry, entity);
        EnsureComponent<HitboxTrackingComponent>(registry, entity);
        LocomotionStateComponent* locomotion = EnsureComponent<LocomotionStateComponent>(registry, entity);
        if (locomotion) {
            EnsureLocomotionRuntimeTuning(*locomotion);
            // Player input is camera-relative stick input.
            locomotion->useCameraRelativeInput = true;
        }
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

        if (auto* collider = registry.GetComponent<ColliderComponent>(entity)) {
            auto& collisionManager = CollisionManager::Instance();
            std::vector<ColliderComponent::Element> persistentElements;
            persistentElements.reserve(collider->elements.size());
            for (auto& element : collider->elements) {
                if (element.registeredId != 0) {
                    collisionManager.Remove(element.registeredId);
                    element.registeredId = 0;
                }
                if (element.runtimeTag == 0) {
                    persistentElements.push_back(element);
                }
            }
            collider->elements.swap(persistentElements);
            collider->enabled = true;
            collider->drawGizmo = true;
        }

        if (auto* hitboxTracking = registry.GetComponent<HitboxTrackingComponent>(entity)) {
            hitboxTracking->ClearHitList();
        }

        if (auto* hitStop = registry.GetComponent<HitStopComponent>(entity)) {
            hitStop->timer = 0.0f;
            hitStop->speedScale = 0.0f;
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

    void EnsureAllPlayerRuntimeComponents(Registry& registry, bool resetRuntimeState)
    {
        std::vector<EntityID> entities;
        CollectEntitiesWithComponent<PlayerTagComponent>(registry, entities);
        CollectEntitiesWithComponent<StateMachineAssetComponent>(registry, entities);
        CollectEntitiesWithComponent<TimelineLibraryComponent>(registry, entities);

        for (EntityID entity : entities) {
            if (Entity::IsNull(entity) || !registry.IsAlive(entity)) {
                continue;
            }

            if (!HasMinimumPlayerAuthoringComponents(registry, entity)) {
                continue;
            }

            EnsurePlayerPersistentComponents(registry, entity);
            EnsurePlayerRuntimeComponents(registry, entity);
            if (resetRuntimeState) {
                ResetPlayerRuntimeState(registry, entity);
            }
        }
    }

    bool HasMinimumPlayerAuthoringComponents(Registry& registry, EntityID entity)
    {
        if (Entity::IsNull(entity) || !registry.IsAlive(entity)) {
            return false;
        }

        return registry.GetComponent<PlayerTagComponent>(entity) != nullptr
            || registry.GetComponent<StateMachineAssetComponent>(entity) != nullptr
            || registry.GetComponent<TimelineLibraryComponent>(entity) != nullptr
            || registry.GetComponent<InputActionMapComponent>(entity) != nullptr
            || registry.GetComponent<StateMachineParamsComponent>(entity) != nullptr
            || registry.GetComponent<InputBindingComponent>(entity) != nullptr
            || registry.GetComponent<InputUserComponent>(entity) != nullptr;
    }
}
