#include "ActionSystem.h"
#include "ActionStateComponent.h"
#include "ActionDatabaseComponent.h"
#include "PlaybackComponent.h"
#include "Input/ResolvedInputStateComponent.h"
#include "Registry/Registry.h"
#include "Component/ComponentSignature.h"
#include "Type/TypeInfo.h"
#include "Archetype/Archetype.h"

namespace InputAction {
    constexpr int AttackLight = 0;
    constexpr int AttackHeavy = 1;
    constexpr int Dodge = 2;
}

void ActionSystem::Update(Registry& registry, float dt) {
    Signature sig = CreateSignature<ActionStateComponent, ActionDatabaseComponent, PlaybackComponent, ResolvedInputStateComponent>();
    for (auto* arch : registry.GetAllArchetypes()) {
        if (!SignatureMatches(arch->GetSignature(), sig)) continue;
        auto* actionCol = arch->GetColumn(TypeManager::GetComponentTypeID<ActionStateComponent>());
        auto* dbCol = arch->GetColumn(TypeManager::GetComponentTypeID<ActionDatabaseComponent>());
        auto* playCol = arch->GetColumn(TypeManager::GetComponentTypeID<PlaybackComponent>());
        auto* inputCol = arch->GetColumn(TypeManager::GetComponentTypeID<ResolvedInputStateComponent>());
        if (!actionCol || !dbCol || !playCol || !inputCol) continue;

        for (size_t i = 0; i < arch->GetEntityCount(); ++i) {
            auto& action = *static_cast<ActionStateComponent*>(actionCol->Get(i));
            auto& db = *static_cast<ActionDatabaseComponent*>(dbCol->Get(i));
            auto& playback = *static_cast<PlaybackComponent*>(playCol->Get(i));
            auto& input = *static_cast<const ResolvedInputStateComponent*>(inputCol->Get(i));

            // Transition: Locomotion -> Action
            if (action.state == CharacterState::Locomotion && action.reservedNodeIndex >= 0) {
                int idx = action.reservedNodeIndex;
                if (idx < db.nodeCount) {
                    action.state = CharacterState::Action;
                    action.currentNodeIndex = idx;
                    action.reservedNodeIndex = -1;
                    action.comboCount++;

                    playback.clipLength = db.nodes[idx].animSpeed > 0.0f ? 1.0f : 1.0f;
                    playback.currentSeconds = 0.0f;
                    playback.playSpeed = db.nodes[idx].animSpeed;
                    playback.playing = true;
                    playback.looping = false;
                    playback.stopAtEnd = true;
                    playback.finished = false;
                }
                continue;
            }

            // In Action state
            if (action.state == CharacterState::Action) {
                if (action.currentNodeIndex < 0 || action.currentNodeIndex >= db.nodeCount) {
                    action.state = CharacterState::Locomotion;
                    continue;
                }

                auto& node = db.nodes[action.currentNodeIndex];
                float t01 = (playback.clipLength > 0.0f) ? playback.currentSeconds / playback.clipLength : 1.0f;

                // Combo input window
                if (t01 >= node.inputStart && t01 <= node.inputEnd) {
                    if (node.nextLight != -1 &&
                        input.actions[InputAction::AttackLight].pressed &&
                        input.actions[InputAction::AttackLight].framesSincePressed < 10) {
                        action.reservedNodeIndex = node.nextLight;
                    }
                    if (node.nextHeavy != -1 &&
                        input.actions[InputAction::AttackHeavy].pressed &&
                        input.actions[InputAction::AttackHeavy].framesSincePressed < 12) {
                        action.reservedNodeIndex = node.nextHeavy;
                    }
                }

                // Commit combo
                if (t01 >= node.comboStart && action.reservedNodeIndex >= 0) {
                    int next = action.reservedNodeIndex;
                    if (next < db.nodeCount) {
                        action.currentNodeIndex = next;
                        action.reservedNodeIndex = -1;
                        action.comboCount++;
                        playback.currentSeconds = 0.0f;
                        playback.playSpeed = db.nodes[next].animSpeed;
                        playback.finished = false;
                        playback.playing = true;
                    }
                    continue;
                }

                // End of clip
                if (playback.finished) {
                    action.state = CharacterState::Locomotion;
                    action.currentNodeIndex = -1;
                    action.reservedNodeIndex = -1;
                }
            }

            // Combo timeout
            if (action.state == CharacterState::Locomotion && action.comboCount > 0) {
                action.comboTimer += dt;
                if (action.comboTimer >= action.comboTimeout) {
                    action.comboCount = 0;
                    action.comboTimer = 0.0f;
                }
            }
        }
    }
}
