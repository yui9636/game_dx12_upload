// ============================================================================
// PlayerEditor — State machine panel, presets, node graph and condition editor.
// Sibling of PlayerEditorPanel.cpp; split out for readability.
// ============================================================================
#include "PlayerEditorPanel.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <algorithm>
#include <cfloat>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <initializer_list>
#include <string>

#include <imgui.h>
#include <imgui_internal.h>

#include "Icon/IconsFontAwesome7.h"
#include "PlayerEditorPanelInternal.h"
#include "PlayerEditorSession.h"
#include "Animator/AnimatorComponent.h"
#include "Gameplay/ActionDatabaseComponent.h"
#include "Gameplay/ActionStateComponent.h"
#include "Gameplay/LocomotionStateComponent.h"
#include "Gameplay/PlaybackComponent.h"
#include "Gameplay/PlayerRuntimeSetup.h"
#include "Gameplay/StateMachineParamsComponent.h"
#include "Model/Model.h"
#include "Registry/Registry.h"

using namespace PlayerEditorInternal;

// ----------------------------------------------------------------------------
// Asset lookup helpers
// ----------------------------------------------------------------------------

StateNode* PlayerEditorPanel::FindStateByName(const char* name)
{
    if (!name || name[0] == '\0') {
        return nullptr;
    }

    for (auto& state : m_stateMachineAsset.states) {
        if (EqualsIgnoreCase(state.name, name)) {
            return &state;
        }
    }
    return nullptr;
}

StateTransition* PlayerEditorPanel::FindTransition(uint32_t fromState, uint32_t toState)
{
    for (auto& transition : m_stateMachineAsset.transitions) {
        if (transition.fromState == fromState && transition.toState == toState) {
            return &transition;
        }
    }
    return nullptr;
}

void PlayerEditorPanel::EnsureStateMachineParameter(const char* name, ParameterType type, float defaultValue)
{
    if (!name || name[0] == '\0') {
        return;
    }

    for (auto& parameter : m_stateMachineAsset.parameters) {
        if (EqualsIgnoreCase(parameter.name, name)) {
            parameter.name = name;
            parameter.type = type;
            parameter.defaultValue = defaultValue;
            return;
        }
    }

    ParameterDef parameter;
    parameter.name = name;
    parameter.type = type;
    parameter.defaultValue = defaultValue;
    m_stateMachineAsset.parameters.push_back(std::move(parameter));
}

// ----------------------------------------------------------------------------
// Animation auto-matching helpers (Spec §12)
// ----------------------------------------------------------------------------

int PlayerEditorPanel::FindAnimationIndexByKeyword(std::initializer_list<const char*> keywords) const
{
    if (!m_model) {
        return -1;
    }

    const auto& animations = m_model->GetAnimations();
    for (int i = 0; i < static_cast<int>(animations.size()); ++i) {
        const std::string loweredName = ToLowerAscii(animations[i].name);
        for (const char* keyword : keywords) {
            if (!keyword || keyword[0] == '\0') {
                continue;
            }

            const std::string loweredKeyword = ToLowerAscii(keyword);
            if (loweredName.find(loweredKeyword) != std::string::npos) {
                return i;
            }
        }
    }

    return -1;
}

bool PlayerEditorPanel::IsNonForwardLocomotionAnimation(int animationIndex) const
{
    if (!m_model) {
        return false;
    }
    const auto& animations = m_model->GetAnimations();
    if (animationIndex < 0 || animationIndex >= static_cast<int>(animations.size())) {
        return false;
    }
    const std::string loweredName = ToLowerAscii(animations[animationIndex].name);
    static const char* kExclude[] = {
        "back", "backward",
        "left", "right",
        "strafe", "side",
        "_l45", "_r45", "_l90", "_r90",
        "turn_l", "turn_r",
    };
    for (const char* keyword : kExclude) {
        if (loweredName.find(keyword) != std::string::npos) {
            return true;
        }
    }
    return false;
}

namespace {
    int FindAnimationByExactKeywordsExcluding(
        const Model* model,
        std::initializer_list<const char*> requiredKeywords,
        bool excludeNonForward,
        std::initializer_list<const char*> alsoExclude = {})
    {
        if (!model) {
            return -1;
        }
        const auto& animations = model->GetAnimations();
        for (int i = 0; i < static_cast<int>(animations.size()); ++i) {
            const std::string loweredName = PlayerEditorInternal::ToLowerAscii(animations[i].name);

            bool hasRequired = false;
            for (const char* keyword : requiredKeywords) {
                if (!keyword || keyword[0] == '\0') continue;
                if (loweredName.find(PlayerEditorInternal::ToLowerAscii(keyword)) != std::string::npos) {
                    hasRequired = true;
                    break;
                }
            }
            if (!hasRequired) continue;

            bool hasExcluded = false;
            for (const char* keyword : alsoExclude) {
                if (!keyword || keyword[0] == '\0') continue;
                if (loweredName.find(PlayerEditorInternal::ToLowerAscii(keyword)) != std::string::npos) {
                    hasExcluded = true;
                    break;
                }
            }
            if (hasExcluded) continue;

            if (excludeNonForward) {
                static const char* kExclude[] = {
                    "back", "backward", "left", "right",
                    "strafe", "side",
                    "_l45", "_r45", "_l90", "_r90",
                    "turn_l", "turn_r",
                };
                bool isNonForward = false;
                for (const char* keyword : kExclude) {
                    if (loweredName.find(keyword) != std::string::npos) {
                        isNonForward = true;
                        break;
                    }
                }
                if (isNonForward) continue;
            }

            return i;
        }
        return -1;
    }
}

int PlayerEditorPanel::FindIdleAnimation() const
{
    return FindAnimationByExactKeywordsExcluding(
        m_model, { "idle" }, false,
        { "turn", "to_jog", "to_run", "to_walk", "trun" });
}

int PlayerEditorPanel::FindWalkAnimation() const
{
    int idx = FindAnimationByExactKeywordsExcluding(
        m_model, { "walk_front", "walk_f" }, true);
    if (idx >= 0) return idx;
    return FindAnimationByExactKeywordsExcluding(
        m_model, { "walk" }, true,
        { "idle", "turn", "trun" });
}

int PlayerEditorPanel::FindJogAnimation() const
{
    int idx = FindAnimationByExactKeywordsExcluding(
        m_model, { "jogging_f", "jog_f" }, true);
    if (idx >= 0) return idx;
    return FindAnimationByExactKeywordsExcluding(
        m_model, { "jog" }, true,
        { "idle", "turn", "trun", "to_" });
}

int PlayerEditorPanel::FindRunAnimation() const
{
    return FindAnimationByExactKeywordsExcluding(
        m_model, { "run" }, true,
        { "fast", "injured", "ldle", "idle", "turn", "trun", "to_" });
}

int PlayerEditorPanel::FindAttackAnimation(int slot) const
{
    if (!m_model) return -1;
    char keyword1[32], keyword2[32], keyword3[32];
    std::snprintf(keyword1, sizeof(keyword1), "combo%d", slot);
    std::snprintf(keyword2, sizeof(keyword2), "combo_%d", slot);
    std::snprintf(keyword3, sizeof(keyword3), "attack%d", slot);

    const auto& animations = m_model->GetAnimations();
    for (int i = 0; i < static_cast<int>(animations.size()); ++i) {
        const std::string loweredName = ToLowerAscii(animations[i].name);
        if (loweredName.find(keyword1) != std::string::npos ||
            loweredName.find(keyword2) != std::string::npos ||
            loweredName.find(keyword3) != std::string::npos) {
            return i;
        }
    }
    return -1;
}

int PlayerEditorPanel::FindDodgeAnimation() const
{
    int idx = FindAnimationByExactKeywordsExcluding(
        m_model, { "dodge_front", "dodge_f" }, false);
    if (idx >= 0) return idx;
    return FindAnimationByExactKeywordsExcluding(
        m_model, { "dodge" }, false,
        { "back" });
}

int PlayerEditorPanel::FindDamageAnimation() const
{
    int idx = FindAnimationByExactKeywordsExcluding(
        m_model, { "damage_front_small", "damage_f_small", "damage_front" }, false);
    if (idx >= 0) return idx;
    return FindAnimationByExactKeywordsExcluding(
        m_model, { "damage" }, false,
        { "down", "knockdown" });
}

// ----------------------------------------------------------------------------
// Preset application
// ----------------------------------------------------------------------------

void PlayerEditorPanel::ApplyLocomotionTransitionPreset(StateTransition& trans, bool enteringMove)
{
    trans.conditions.clear();
    trans.priority = 100;
    trans.hasExitTime = false;
    trans.exitTimeNormalized = 0.0f;
    trans.blendDuration = 0.15f;

    TransitionCondition condition;
    condition.type = ConditionType::Parameter;
    strncpy_s(condition.param, enteringMove ? "IsMoving" : "IsMoving", _TRUNCATE);
    condition.compare = enteringMove ? CompareOp::GreaterEqual : CompareOp::LessEqual;
    condition.value = enteringMove ? 1.0f : 0.0f;
    trans.conditions.push_back(condition);
}

// Spec §6: Setup Full Player creates Idle / Walk / Jog / Run (loop=true).
// Spec §7.1: Gait-based transitions (6 total).
// Spec §3.3: Idempotent — find by name, only add if missing.
void PlayerEditorPanel::ApplyLocomotionStateMachinePreset()
{
    EnsureStateMachineParameter("MoveX", ParameterType::Float, 0.0f);
    EnsureStateMachineParameter("MoveY", ParameterType::Float, 0.0f);
    EnsureStateMachineParameter("MoveMagnitude", ParameterType::Float, 0.0f);
    EnsureStateMachineParameter("IsMoving", ParameterType::Bool, 0.0f);
    EnsureStateMachineParameter("Gait", ParameterType::Int, 0.0f);
    EnsureStateMachineParameter("IsWalking", ParameterType::Bool, 0.0f);
    EnsureStateMachineParameter("IsRunning", ParameterType::Bool, 0.0f);
    EnsureStateMachineParameter("Attack", ParameterType::Trigger, 0.0f);
    EnsureStateMachineParameter("Dodge", ParameterType::Trigger, 0.0f);
    EnsureStateMachineParameter("Damaged", ParameterType::Trigger, 0.0f);

    struct LocoStateDef {
        const char* name;
        DirectX::XMFLOAT2 defaultPos;
        int (PlayerEditorPanel::*resolver)() const;
    };
    const LocoStateDef defs[] = {
        { "Idle", {   0.0f, 0.0f }, &PlayerEditorPanel::FindIdleAnimation },
        { "Walk", { 220.0f, 0.0f }, &PlayerEditorPanel::FindWalkAnimation },
        { "Jog",  { 440.0f, 0.0f }, &PlayerEditorPanel::FindJogAnimation  },
        { "Run",  { 660.0f, 0.0f }, &PlayerEditorPanel::FindRunAnimation  },
    };

    uint32_t ids[4] = { 0, 0, 0, 0 };
    for (int i = 0; i < 4; ++i) {
        StateNode* node = FindStateByName(defs[i].name);
        if (!node) {
            node = m_stateMachineAsset.AddState(defs[i].name, StateNodeType::Locomotion);
        }
        if (!node) continue;

        node->name = defs[i].name;
        node->type = StateNodeType::Locomotion;
        node->loopAnimation = true;
        node->canInterrupt = true;
        node->animSpeed = 1.0f;
        if (node->position.x == 0.0f && node->position.y == 0.0f) {
            node->position = defs[i].defaultPos;
        }
        if (node->animationIndex < 0 || IsNonForwardLocomotionAnimation(node->animationIndex)) {
            node->animationIndex = (this->*defs[i].resolver)();
        }
        ids[i] = node->id;
    }

    if (ids[0] != 0) {
        m_stateMachineAsset.defaultStateId = ids[0];
    }

    auto applyGaitTransition = [&](uint32_t fromId, uint32_t toId, int gaitThreshold, bool entering) {
        if (fromId == 0 || toId == 0) return;
        StateTransition* t = FindTransition(fromId, toId);
        if (!t) {
            t = m_stateMachineAsset.AddTransition(fromId, toId);
        }
        if (!t) return;
        t->conditions.clear();
        t->priority = 100;
        t->hasExitTime = false;
        t->exitTimeNormalized = 0.0f;
        t->blendDuration = 0.15f;

        TransitionCondition cond;
        cond.type = ConditionType::Parameter;
        strncpy_s(cond.param, "Gait", _TRUNCATE);
        cond.compare = entering ? CompareOp::GreaterEqual : CompareOp::LessEqual;
        cond.value = static_cast<float>(gaitThreshold);
        t->conditions.push_back(cond);
    };

    // Idle <-> Walk (Gait 1)
    applyGaitTransition(ids[0], ids[1], 1, true);
    applyGaitTransition(ids[1], ids[0], 0, false);
    // Walk <-> Jog (Gait 2)
    applyGaitTransition(ids[1], ids[2], 2, true);
    applyGaitTransition(ids[2], ids[1], 1, false);
    // Jog <-> Run (Gait 3)
    applyGaitTransition(ids[2], ids[3], 3, true);
    applyGaitTransition(ids[3], ids[2], 2, false);

    m_selectedNodeId = ids[0];
    m_selectedTransitionId = 0;
    m_selectionCtx = SelectionContext::StateNode;
    m_stateMachineDirty = true;
}

// Spec §6: Setup Full Player creates Attack1〜3 (Action, loop=false).
// Spec §7.2 / §7.5 / §7.7: Loco→Atk1 (×4), Atk1→Atk2, Atk2→Atk3, Atk{1,2,3}→Idle (AnimEnd).
// Spec §11: ActionDatabase node index links via properties["ActionNodeIndex"].
void PlayerEditorPanel::ApplyAttackComboPreset()
{
    EnsureStateMachineParameter("Attack", ParameterType::Trigger, 0.0f);

    StateNode* idle = FindStateByName("Idle");
    StateNode* walk = FindStateByName("Walk");
    StateNode* jog  = FindStateByName("Jog");
    StateNode* run  = FindStateByName("Run");

    struct AttackDef {
        const char* name;
        int slot;
        DirectX::XMFLOAT2 defaultPos;
    };
    const AttackDef defs[3] = {
        { "Attack1", 1, {  0.0f, 220.0f } },
        { "Attack2", 2, { 240.0f, 220.0f } },
        { "Attack3", 3, { 480.0f, 220.0f } },
    };

    uint32_t attackIds[3] = { 0, 0, 0 };
    for (int i = 0; i < 3; ++i) {
        StateNode* node = FindStateByName(defs[i].name);
        if (!node) {
            node = m_stateMachineAsset.AddState(defs[i].name, StateNodeType::Action);
        }
        if (!node) continue;

        node->name = defs[i].name;
        node->type = StateNodeType::Action;
        node->loopAnimation = false;
        node->canInterrupt = false;
        node->animSpeed = 1.0f;
        if (node->position.x == 0.0f && node->position.y == 0.0f) {
            node->position = defs[i].defaultPos;
        }
        if (node->animationIndex < 0) {
            node->animationIndex = FindAttackAnimation(defs[i].slot);
        }
        node->properties["ActionNodeIndex"] = static_cast<float>(i);
        attackIds[i] = node->id;
    }

    auto setAttackTrigger = [&](StateTransition& t) {
        t.conditions.clear();
        t.priority = 200;
        t.hasExitTime = false;
        t.exitTimeNormalized = 0.0f;
        t.blendDuration = 0.08f;
        TransitionCondition cond;
        cond.type = ConditionType::Parameter;
        strncpy_s(cond.param, "Attack", _TRUNCATE);
        cond.compare = CompareOp::Equal;
        cond.value = 1.0f;
        t.conditions.push_back(cond);
    };

    auto setAttackTriggerWithExitTime = [&](StateTransition& t, float exitTime) {
        setAttackTrigger(t);
        t.hasExitTime = true;
        t.exitTimeNormalized = exitTime;
    };

    auto setAnimEnd = [&](StateTransition& t) {
        t.conditions.clear();
        t.priority = 100;
        t.hasExitTime = false;
        t.exitTimeNormalized = 0.0f;
        t.blendDuration = 0.12f;
        TransitionCondition cond;
        cond.type = ConditionType::AnimEnd;
        cond.compare = CompareOp::Equal;
        cond.value = 1.0f;
        t.conditions.push_back(cond);
    };

    auto upsertTransition = [&](uint32_t fromId, uint32_t toId, auto&& configure) {
        if (fromId == 0 || toId == 0) return;
        StateTransition* t = FindTransition(fromId, toId);
        if (!t) {
            t = m_stateMachineAsset.AddTransition(fromId, toId);
        }
        if (t) configure(*t);
    };

    // §7.2: Locomotion -> Attack1 (4 transitions)
    StateNode* locos[4] = { idle, walk, jog, run };
    for (StateNode* loco : locos) {
        if (loco) upsertTransition(loco->id, attackIds[0], setAttackTrigger);
    }

    // §7.5: Attack combo (Attack1->Attack2, Attack2->Attack3)
    upsertTransition(attackIds[0], attackIds[1], [&](StateTransition& t) { setAttackTriggerWithExitTime(t, 0.4f); });
    upsertTransition(attackIds[1], attackIds[2], [&](StateTransition& t) { setAttackTriggerWithExitTime(t, 0.4f); });

    // §7.7: Attack{1,2,3} -> Idle (AnimEnd)
    if (idle) {
        for (int i = 0; i < 3; ++i) {
            upsertTransition(attackIds[i], idle->id, setAnimEnd);
        }
    }

    m_stateMachineDirty = true;
}

// Spec §6: Setup Full Player creates Dodge (Type Dodge, loop=false).
// Spec §7.3 / §7.6 / §7.7: Loco→Dodge (×4), Atk→Dodge with cancel exitTime (×3), Dodge→Idle (AnimEnd).
void PlayerEditorPanel::ApplyDodgePreset()
{
    EnsureStateMachineParameter("Dodge", ParameterType::Trigger, 0.0f);

    StateNode* idle = FindStateByName("Idle");
    StateNode* walk = FindStateByName("Walk");
    StateNode* jog  = FindStateByName("Jog");
    StateNode* run  = FindStateByName("Run");
    StateNode* atk1 = FindStateByName("Attack1");
    StateNode* atk2 = FindStateByName("Attack2");
    StateNode* atk3 = FindStateByName("Attack3");

    StateNode* dodge = FindStateByName("Dodge");
    if (!dodge) {
        dodge = m_stateMachineAsset.AddState("Dodge", StateNodeType::Dodge);
    }
    if (!dodge) return;

    dodge->name = "Dodge";
    dodge->type = StateNodeType::Dodge;
    dodge->loopAnimation = false;
    dodge->canInterrupt = false;
    dodge->animSpeed = 1.0f;
    if (dodge->position.x == 0.0f && dodge->position.y == 0.0f) {
        dodge->position = { 720.0f, 220.0f };
    }
    if (dodge->animationIndex < 0) {
        dodge->animationIndex = FindDodgeAnimation();
    }

    auto setDodgeTrigger = [&](StateTransition& t, float exitTime) {
        t.conditions.clear();
        t.priority = 300;
        t.hasExitTime = exitTime > 0.0f;
        t.exitTimeNormalized = exitTime;
        t.blendDuration = 0.08f;
        TransitionCondition cond;
        cond.type = ConditionType::Parameter;
        strncpy_s(cond.param, "Dodge", _TRUNCATE);
        cond.compare = CompareOp::Equal;
        cond.value = 1.0f;
        t.conditions.push_back(cond);
    };

    auto setAnimEnd = [&](StateTransition& t) {
        t.conditions.clear();
        t.priority = 100;
        t.hasExitTime = false;
        t.exitTimeNormalized = 0.0f;
        t.blendDuration = 0.12f;
        TransitionCondition cond;
        cond.type = ConditionType::AnimEnd;
        cond.compare = CompareOp::Equal;
        cond.value = 1.0f;
        t.conditions.push_back(cond);
    };

    auto upsertTransition = [&](uint32_t fromId, uint32_t toId, auto&& configure) {
        if (fromId == 0 || toId == 0) return;
        StateTransition* t = FindTransition(fromId, toId);
        if (!t) {
            t = m_stateMachineAsset.AddTransition(fromId, toId);
        }
        if (t) configure(*t);
    };

    // §7.3: Locomotion -> Dodge (4)
    StateNode* locos[4] = { idle, walk, jog, run };
    for (StateNode* loco : locos) {
        if (loco) upsertTransition(loco->id, dodge->id, [&](StateTransition& t) { setDodgeTrigger(t, 0.0f); });
    }

    // §7.6: Action -> Dodge cancel (3) with exitTime>=0.2
    StateNode* atks[3] = { atk1, atk2, atk3 };
    for (StateNode* atk : atks) {
        if (atk) upsertTransition(atk->id, dodge->id, [&](StateTransition& t) { setDodgeTrigger(t, 0.2f); });
    }

    // §7.7: Dodge -> Idle (AnimEnd)
    if (idle) upsertTransition(dodge->id, idle->id, setAnimEnd);

    m_stateMachineDirty = true;
}

// Spec §6: Setup Full Player creates Damage (Type Damage, loop=false).
// Spec §7.4 / §7.7: 7 transitions Loco/Action -> Damage (Damaged==1, priority 500), 1 Damage -> Idle (AnimEnd).
void PlayerEditorPanel::ApplyDamagePreset()
{
    EnsureStateMachineParameter("Damaged", ParameterType::Trigger, 0.0f);

    StateNode* idle = FindStateByName("Idle");
    StateNode* walk = FindStateByName("Walk");
    StateNode* jog  = FindStateByName("Jog");
    StateNode* run  = FindStateByName("Run");
    StateNode* atk1 = FindStateByName("Attack1");
    StateNode* atk2 = FindStateByName("Attack2");
    StateNode* atk3 = FindStateByName("Attack3");

    StateNode* damage = FindStateByName("Damage");
    if (!damage) {
        damage = m_stateMachineAsset.AddState("Damage", StateNodeType::Damage);
    }
    if (!damage) return;

    damage->name = "Damage";
    damage->type = StateNodeType::Damage;
    damage->loopAnimation = false;
    damage->canInterrupt = false;
    damage->animSpeed = 1.0f;
    if (damage->position.x == 0.0f && damage->position.y == 0.0f) {
        damage->position = { 720.0f, 440.0f };
    }
    if (damage->animationIndex < 0) {
        damage->animationIndex = FindDamageAnimation();
    }

    auto setDamagedTrigger = [&](StateTransition& t) {
        t.conditions.clear();
        t.priority = 500;
        t.hasExitTime = false;
        t.exitTimeNormalized = 0.0f;
        t.blendDuration = 0.05f;
        TransitionCondition cond;
        cond.type = ConditionType::Parameter;
        strncpy_s(cond.param, "Damaged", _TRUNCATE);
        cond.compare = CompareOp::Equal;
        cond.value = 1.0f;
        t.conditions.push_back(cond);
    };

    auto setAnimEnd = [&](StateTransition& t) {
        t.conditions.clear();
        t.priority = 100;
        t.hasExitTime = false;
        t.exitTimeNormalized = 0.0f;
        t.blendDuration = 0.15f;
        TransitionCondition cond;
        cond.type = ConditionType::AnimEnd;
        cond.compare = CompareOp::Equal;
        cond.value = 1.0f;
        t.conditions.push_back(cond);
    };

    auto upsertTransition = [&](uint32_t fromId, uint32_t toId, auto&& configure) {
        if (fromId == 0 || toId == 0) return;
        StateTransition* t = FindTransition(fromId, toId);
        if (!t) {
            t = m_stateMachineAsset.AddTransition(fromId, toId);
        }
        if (t) configure(*t);
    };

    // §7.4: Loco/Action -> Damage (7 transitions). Dodge is excluded (invincibility).
    StateNode* sources[7] = { idle, walk, jog, run, atk1, atk2, atk3 };
    for (StateNode* source : sources) {
        if (source) upsertTransition(source->id, damage->id, setDamagedTrigger);
    }

    // §7.7: Damage -> Idle (AnimEnd)
    if (idle) upsertTransition(damage->id, idle->id, setAnimEnd);

    m_stateMachineDirty = true;
}

// Remove transitions whose fromState or toState does not correspond to any
// existing state in the asset. This purges stale entries written by older code
// versions that could leave garbage IDs (e.g. 0xDDDDDDDD) blocking correct
// transitions added by the preset functions that follow.
void PlayerEditorPanel::RemoveBrokenTransitions()
{
    auto isValidId = [&](uint32_t id) {
        for (const auto& s : m_stateMachineAsset.states) {
            if (s.id == id) return true;
        }
        return false;
    };

    auto& transitions = m_stateMachineAsset.transitions;
    transitions.erase(
        std::remove_if(transitions.begin(), transitions.end(),
            [&](const StateTransition& t) {
                return !isValidId(t.fromState) || !isValidId(t.toState);
            }),
        transitions.end());
}

// Spec §1: Setup Full Player composes all sub-presets in one click.
// Spec §3.3: Idempotent — running twice yields the same counts.
void PlayerEditorPanel::ApplyFullPlayerPreset()
{
    // Purge any stale transitions referencing dead state IDs (e.g. 0xDDDDDDDD
    // legacy debug-heap garbage) BEFORE the presets run. Otherwise broken
    // entries can shadow the freshly-added ones during runtime evaluation
    // (transitions with equal priority resolve in vector order, so a broken
    // entry sitting earlier wins and silently blocks the correct path).
    RemoveBrokenTransitions();

    ApplyLocomotionStateMachinePreset();
    ApplyAttackComboPreset();
    ApplyDodgePreset();
    ApplyDamagePreset();

    // Final sweep — the presets only ever insert valid transitions, but call
    // this again so the asset is guaranteed clean even if a future preset
    // mistakenly forwards a stale id.
    RemoveBrokenTransitions();

    InputActionMapAsset& inputMap = m_inputMappingTab.GetEditingMapMutable();
    if (EnsurePlayerInputMap(inputMap)) {
        m_inputMappingTab.MarkDirty();
    }

    if (CanUsePreviewEntity()) {
        ApplyEditorBindingsToPreviewEntity();
        if (ActionDatabaseComponent* database = m_registry->GetComponent<ActionDatabaseComponent>(m_previewEntity)) {
            struct ResolverCtx { const PlayerEditorPanel* self; };
            ResolverCtx ctx{ this };
            EnsureAttackComboActionNodes(*database,
                [](int slot, void* user) -> int {
                    return static_cast<ResolverCtx*>(user)->self->FindAttackAnimation(slot);
                },
                &ctx);
        }
        PlayerRuntimeSetup::ResetPlayerRuntimeState(*m_registry, m_previewEntity);
    }

    StateNode* idle = FindStateByName("Idle");
    if (idle) {
        m_selectedNodeId = idle->id;
        m_selectionCtx = SelectionContext::StateNode;
    }
    m_selectedTransitionId = 0;
    m_stateMachineDirty = true;
}

// ----------------------------------------------------------------------------
// Runtime status panel (sidebar inside Properties when nothing is selected)
// ----------------------------------------------------------------------------

void PlayerEditorPanel::DrawStateMachineRuntimeStatus()
{
    ImGui::Text(ICON_FA_PERSON_RUNNING " Preview Runtime");
    ImGui::Separator();

    if (!CanUsePreviewEntity()) {
        m_runtimeObservedStateId = 0;
        m_runtimePreviousStateId = 0;
        m_runtimeLastTransitionLabel.clear();
        ImGui::TextDisabled("Preview entity is not available.");
        return;
    }

    const StateMachineParamsComponent* params = m_registry->GetComponent<StateMachineParamsComponent>(m_previewEntity);
    if (!params) {
        m_runtimeObservedStateId = 0;
        m_runtimePreviousStateId = 0;
        m_runtimeLastTransitionLabel.clear();
        ImGui::TextDisabled("StateMachineParamsComponent is missing.");
        return;
    }

    const uint32_t currentStateId = params->currentStateId;
    if (currentStateId == 0) {
        m_runtimeObservedStateId = 0;
        m_runtimePreviousStateId = 0;
        m_runtimeLastTransitionLabel.clear();
    } else if (m_runtimeObservedStateId != currentStateId) {
        if (m_runtimeObservedStateId != 0) {
            m_runtimePreviousStateId = m_runtimeObservedStateId;
            const StateNode* previousState = m_stateMachineAsset.FindState(m_runtimeObservedStateId);
            const StateNode* currentState = m_stateMachineAsset.FindState(currentStateId);
            const char* previousName = previousState ? previousState->name.c_str() : "Unknown";
            const char* currentName = currentState ? currentState->name.c_str() : "Unknown";
            m_runtimeLastTransitionLabel = std::string(previousName) + " -> " + currentName;
        }
        m_runtimeObservedStateId = currentStateId;
    }

    const StateNode* currentState = m_stateMachineAsset.FindState(currentStateId);
    const StateNode* previousState = m_stateMachineAsset.FindState(m_runtimePreviousStateId);
    const AnimatorComponent* animator = m_registry->GetComponent<AnimatorComponent>(m_previewEntity);
    const LocomotionStateComponent* locomotion = m_registry->GetComponent<LocomotionStateComponent>(m_previewEntity);
    const ActionStateComponent* actionState = m_registry->GetComponent<ActionStateComponent>(m_previewEntity);

    std::string currentAnimation = "(none)";
    if (animator && m_model) {
        const int animationIndex = animator->baseLayer.currentAnimIndex;
        const auto& animations = m_model->GetAnimations();
        if (animationIndex >= 0 && animationIndex < static_cast<int>(animations.size())) {
            currentAnimation = "[" + std::to_string(animationIndex) + "] " + animations[animationIndex].name;
        } else if (animationIndex >= 0) {
            currentAnimation = "[" + std::to_string(animationIndex) + "]";
        }
    }

    ImGui::Text("Current State: %s", currentState ? currentState->name.c_str() : "(none)");
    ImGui::Text("Previous State: %s", previousState ? previousState->name.c_str() : "(none)");
    ImGui::Text("Last Transition: %s", m_runtimeLastTransitionLabel.empty() ? "(none)" : m_runtimeLastTransitionLabel.c_str());
    ImGui::Text("State Timer: %.2fs", params->stateTimer);
    ImGui::Text("Animation: %s", currentAnimation.c_str());
    if (actionState && actionState->state == CharacterState::Action) {
        const int idx = actionState->currentNodeIndex;
        std::string actionLabel;
        if (idx >= 0 && idx < 3) {
            actionLabel = "Attack" + std::to_string(idx + 1);
        } else if (idx >= 0) {
            actionLabel = "Node " + std::to_string(idx);
        } else {
            actionLabel = "(unset)";
        }
        ImGui::Text("Current Action: %s", actionLabel.c_str());
    } else {
        ImGui::Text("Current Action: (none)");
    }

    const float moveX = params->GetParam("MoveX");
    const float moveY = params->GetParam("MoveY");
    const float moveMagnitude = params->GetParam("MoveMagnitude");
    const float isMoving = params->GetParam("IsMoving");
    const float gait = params->GetParam("Gait");
    const float isWalking = params->GetParam("IsWalking");
    const float isRunning = params->GetParam("IsRunning");
    ImGui::Separator();
    ImGui::Text("MoveX: %.3f", moveX);
    ImGui::Text("MoveY: %.3f", moveY);
    ImGui::Text("MoveMagnitude: %.3f", moveMagnitude);
    ImGui::Text("IsMoving: %.0f", isMoving);
    ImGui::Text("Gait: %.0f", gait);
    ImGui::Text("IsWalking: %.0f", isWalking);
    ImGui::Text("IsRunning: %.0f", isRunning);
    if (locomotion) {
        ImGui::TextDisabled("Gait: %u  InputStrength: %.3f", static_cast<unsigned>(locomotion->gaitIndex), locomotion->inputStrength);
    }
}

// ----------------------------------------------------------------------------
// State Machine Panel & node graph
// ----------------------------------------------------------------------------

void PlayerEditorPanel::DrawStateMachinePanel()
{
    if (!ImGui::Begin(kPEStateMachineTitle)) { ImGui::End(); return; }

    const float panelWidth = ImGui::GetContentRegionAvail().x;
    static float stateListWidth = 320.0f;
    float minListWidth = 280.0f;
    float maxListWidth = panelWidth - 260.0f;
    if (maxListWidth < minListWidth) {
        maxListWidth = minListWidth;
    }
    if (stateListWidth < minListWidth) {
        stateListWidth = minListWidth;
    }
    if (stateListWidth > maxListWidth) {
        stateListWidth = maxListWidth;
    }
    const bool hasSelectedState = m_stateMachineAsset.FindState(m_selectedNodeId) != nullptr;

    if (ImGui::Button(ICON_FA_PLUS " Add State")) {
        ImGui::OpenPopup("AddStateTemplatePopup");
    }
    ImGui::SameLine();
    if (ImGui::Button(ICON_FA_PERSON_RUNNING " Setup Full Player")) {
        ApplyFullPlayerPreset();
    }
    ImGui::SameLine();
    if (DrawToolbarButton(ICON_FA_PLAY " Preview State", hasSelectedState)) {
        PreviewStateNode(m_selectedNodeId, true);
    }
    ImGui::SameLine();
    if (ImGui::Button(ICON_FA_ARROWS_TO_CIRCLE " Fit")) {
        // The actual fit math runs inside DrawNodeGraph where the canvas
        // size is known; flagging a request keeps the camera computation
        // colocated with the graph rendering.
        m_graphFitRequested = true;
    }
    ImGui::SameLine();
    ImGui::SetNextItemWidth(80);
    ImGui::SliderFloat("Zoom##SM", &m_graphZoom, 0.3f, 3.0f, "%.1f");
    ImGui::SameLine();
    if (hasSelectedState && ImGui::Button(ICON_FA_STAR " Default")) {
        m_stateMachineAsset.defaultStateId = m_selectedNodeId;
        m_stateMachineDirty = true;
    }

    if (m_isConnecting) {
        ImGui::TextColored(ImVec4(1, 0.8f, 0.2f, 1), ICON_FA_LINK " Connecting... (ESC cancel)");
        if (ImGui::IsKeyPressed(ImGuiKey_Escape)) {
            m_isConnecting = false;
            m_connectFromNodeId = 0;
        }
    }

    if (ImGui::BeginPopup("AddStateTemplatePopup")) {
        ImGui::TextDisabled("Presets:");
        if (ImGui::MenuItem("Setup Full Player"))     ApplyFullPlayerPreset();
        if (ImGui::MenuItem("Setup Locomotion Only")) ApplyLocomotionStateMachinePreset();
        if (ImGui::MenuItem("Add Attack Combo"))      ApplyAttackComboPreset();
        if (ImGui::MenuItem("Add Dodge"))             ApplyDodgePreset();
        if (ImGui::MenuItem("Add Damage"))            ApplyDamagePreset();
        ImGui::Separator();
        ImGui::TextDisabled("Single State:");
        if (ImGui::MenuItem("Locomotion")) AddStateTemplate(StateNodeType::Locomotion, { 0.0f, 0.0f });
        if (ImGui::MenuItem("Action"))     AddStateTemplate(StateNodeType::Action, { 0.0f, 0.0f });
        if (ImGui::MenuItem("Dodge"))      AddStateTemplate(StateNodeType::Dodge, { 0.0f, 0.0f });
        if (ImGui::MenuItem("Jump"))       AddStateTemplate(StateNodeType::Jump, { 0.0f, 0.0f });
        if (ImGui::MenuItem("Damage"))     AddStateTemplate(StateNodeType::Damage, { 0.0f, 0.0f });
        if (ImGui::MenuItem("Dead"))       AddStateTemplate(StateNodeType::Dead, { 0.0f, 0.0f });
        if (ImGui::MenuItem("Custom"))     AddStateTemplate(StateNodeType::Custom, { 0.0f, 0.0f });
        ImGui::EndPopup();
    }

    ImGui::Separator();

    ImGui::BeginChild("StateListPane", ImVec2(stateListWidth, 0.0f), ImGuiChildFlags_Borders);
    ImGui::Text(ICON_FA_LIST " States (%d)", static_cast<int>(m_stateMachineAsset.states.size()));
    ImGui::Separator();

    // Each row: a single Selectable spans the full entry height so the click
    // hit-area covers name + sub-info, with text drawn on top via overlap so
    // the state name and metadata are clearly readable (the previous compact
    // layout vertically clipped the name).
    constexpr float kEntryHeight   = 56.0f;
    constexpr float kColorBarWidth = 4.0f;
    constexpr float kPaddingX      = 10.0f;
    constexpr float kNameY         = 6.0f;
    constexpr float kSubY          = 24.0f;
    constexpr float kAnimY         = 40.0f;

    for (const auto& state : m_stateMachineAsset.states) {
        ImGui::PushID(static_cast<int>(state.id));

        const bool selected = (m_selectedNodeId == state.id);
        const std::string hiddenId = "##state_row_" + std::to_string(state.id);
        if (ImGui::Selectable(hiddenId.c_str(), selected,
            ImGuiSelectableFlags_AllowOverlap, ImVec2(-FLT_MIN, kEntryHeight))) {
            m_selectedNodeId = state.id;
            m_selectedTransitionId = 0;
            m_selectionCtx = SelectionContext::StateNode;
        }
        if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0)) {
            PreviewStateNode(state.id, true);
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("%s\nDouble-click to preview", state.name.c_str());
        }

        // Draw on top of the selectable.
        const ImVec2 itemMin = ImGui::GetItemRectMin();
        const ImVec2 itemMax = ImGui::GetItemRectMax();
        ImDrawList* dl = ImGui::GetWindowDrawList();

        // Left color bar — gives a quick visual hint of the state type.
        dl->AddRectFilled(
            itemMin,
            ImVec2(itemMin.x + kColorBarWidth, itemMax.y),
            StateNodeColor(state.type));

        const float textX = itemMin.x + kColorBarWidth + kPaddingX;

        // State name (primary).
        dl->AddText(
            ImVec2(textX, itemMin.y + kNameY),
            ImGui::GetColorU32(ImGuiCol_Text),
            state.name.c_str());

        // Default badge to the right of the name.
        if (m_stateMachineAsset.defaultStateId == state.id) {
            const float nameWidth = ImGui::CalcTextSize(state.name.c_str()).x;
            dl->AddText(
                ImVec2(textX + nameWidth + 8.0f, itemMin.y + kNameY),
                IM_COL32(255, 200, 60, 255),
                "[Default]");
        }

        // Type / outgoing-transition counter.
        const int outgoing = static_cast<int>(m_stateMachineAsset.GetTransitionsFrom(state.id).size());
        char subBuffer[96];
        std::snprintf(subBuffer, sizeof(subBuffer), "%s  |  %d transition%s",
            GetStateTypeLabel(state.type),
            outgoing,
            outgoing == 1 ? "" : "s");
        dl->AddText(
            ImVec2(textX, itemMin.y + kSubY),
            ImGui::GetColorU32(ImGuiCol_TextDisabled),
            subBuffer);

        // Animation row.
        const char* animationText = "(No Animation)";
        if (m_model
            && state.animationIndex >= 0
            && state.animationIndex < static_cast<int>(m_model->GetAnimations().size())) {
            animationText = m_model->GetAnimations()[state.animationIndex].name.c_str();
        }
        dl->AddText(
            ImVec2(textX, itemMin.y + kAnimY),
            ImGui::GetColorU32(ImGuiCol_TextDisabled),
            animationText);

        // Bottom hairline separator drawn manually so the rounded corners of
        // the Selectable are not interrupted by ImGui::Separator().
        dl->AddLine(
            ImVec2(itemMin.x, itemMax.y - 0.5f),
            ImVec2(itemMax.x, itemMax.y - 0.5f),
            ImGui::GetColorU32(ImGuiCol_Separator));

        ImGui::PopID();
    }
    ImGui::EndChild();

    ImGui::SameLine();
    ImGui::InvisibleButton("StateListSplitter", ImVec2(6.0f, ImGui::GetContentRegionAvail().y));
    if (ImGui::IsItemActive()) {
        stateListWidth += ImGui::GetIO().MouseDelta.x;
        if (stateListWidth < minListWidth) {
            stateListWidth = minListWidth;
        }
        if (stateListWidth > maxListWidth) {
            stateListWidth = maxListWidth;
        }
    }
    if (ImGui::IsItemHovered() || ImGui::IsItemActive()) {
        ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
    }

    ImGui::SameLine();
    ImGui::BeginChild("StateMachineGraphPane", ImVec2(0.0f, 0.0f), false);
    ImVec2 canvasSize = ImGui::GetContentRegionAvail();
    DrawNodeGraph(canvasSize);
    ImGui::EndChild();

    ImGui::End();
}

void PlayerEditorPanel::FitGraphToContent(const ImVec2& canvasSize)
{
    if (m_stateMachineAsset.states.empty() || canvasSize.x <= 0.0f || canvasSize.y <= 0.0f) {
        m_graphOffset = { 200.0f, 150.0f };
        m_graphZoom = 1.0f;
        return;
    }

    float minX = FLT_MAX, minY = FLT_MAX;
    float maxX = -FLT_MAX, maxY = -FLT_MAX;
    for (const auto& state : m_stateMachineAsset.states) {
        minX = (std::min)(minX, state.position.x);
        minY = (std::min)(minY, state.position.y);
        maxX = (std::max)(maxX, state.position.x + kNodeWidth);
        maxY = (std::max)(maxY, state.position.y + kNodeHeight);
    }

    constexpr float kPadding = 60.0f;
    const float contentWidth  = (std::max)(1.0f, maxX - minX);
    const float contentHeight = (std::max)(1.0f, maxY - minY);
    const float zoomX = (canvasSize.x - kPadding * 2.0f) / contentWidth;
    const float zoomY = (canvasSize.y - kPadding * 2.0f) / contentHeight;
    float zoom = (std::min)(zoomX, zoomY);
    zoom = (std::max)(0.2f, (std::min)(2.0f, zoom));

    m_graphZoom = zoom;
    m_graphOffset.x = kPadding - minX * zoom + (canvasSize.x - kPadding * 2.0f - contentWidth * zoom) * 0.5f;
    m_graphOffset.y = kPadding - minY * zoom + (canvasSize.y - kPadding * 2.0f - contentHeight * zoom) * 0.5f;
}

void PlayerEditorPanel::DrawNodeGraph(ImVec2 canvasSize)
{
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 origin = ImGui::GetCursorScreenPos();

    if (m_graphFitRequested) {
        FitGraphToContent(canvasSize);
        m_graphFitRequested = false;
    }

    // Background
    dl->AddRectFilled(origin, ImVec2(origin.x + canvasSize.x, origin.y + canvasSize.y),
        IM_COL32(22, 22, 28, 255));

    // Grid
    float gridStep = 32.0f * m_graphZoom;
    if (gridStep > 4.0f) {
        for (float x = fmodf(m_graphOffset.x, gridStep); x < canvasSize.x; x += gridStep)
            dl->AddLine(ImVec2(origin.x + x, origin.y), ImVec2(origin.x + x, origin.y + canvasSize.y),
                IM_COL32(40, 40, 48, 255));
        for (float y = fmodf(m_graphOffset.y, gridStep); y < canvasSize.y; y += gridStep)
            dl->AddLine(ImVec2(origin.x, origin.y + y), ImVec2(origin.x + canvasSize.x, origin.y + y),
                IM_COL32(40, 40, 48, 255));
    }

    auto NodeScreenPos = [&](const StateNode& s) -> ImVec2 {
        return ImVec2(
            origin.x + m_graphOffset.x + s.position.x * m_graphZoom,
            origin.y + m_graphOffset.y + s.position.y * m_graphZoom);
    };
    auto NodeScreenCenter = [&](const StateNode& s) -> ImVec2 {
        auto p = NodeScreenPos(s);
        return ImVec2(p.x + kNodeWidth * 0.5f * m_graphZoom, p.y + kNodeHeight * 0.5f * m_graphZoom);
    };

    // ── Draw transitions (arrows) ──
    for (auto& trans : m_stateMachineAsset.transitions) {
        auto* from = m_stateMachineAsset.FindState(trans.fromState);
        auto* to   = m_stateMachineAsset.FindState(trans.toState);
        if (!from || !to) continue;

        ImVec2 p1 = NodeScreenCenter(*from);
        ImVec2 p2 = NodeScreenCenter(*to);

        bool isSel = (m_selectedTransitionId == trans.id);
        ImU32 lineCol = isSel ? IM_COL32(255, 255, 80, 255) : IM_COL32(180, 180, 180, 160);
        float thickness = isSel ? 3.0f : 2.0f;
        dl->AddLine(p1, p2, lineCol, thickness);

        // Arrowhead at midpoint
        ImVec2 dir(p2.x - p1.x, p2.y - p1.y);
        float len = sqrtf(dir.x * dir.x + dir.y * dir.y);
        if (len > 1.0f) {
            dir.x /= len; dir.y /= len;
            ImVec2 mid((p1.x + p2.x) * 0.5f, (p1.y + p2.y) * 0.5f);
            ImVec2 perp(-dir.y, dir.x);
            float as = 10.0f;
            dl->AddTriangleFilled(
                ImVec2(mid.x + dir.x * as, mid.y + dir.y * as),
                ImVec2(mid.x - dir.x * as * 0.6f + perp.x * as * 0.5f, mid.y - dir.y * as * 0.6f + perp.y * as * 0.5f),
                ImVec2(mid.x - dir.x * as * 0.6f - perp.x * as * 0.5f, mid.y - dir.y * as * 0.6f - perp.y * as * 0.5f),
                lineCol);

            // Click on midpoint to select transition
            ImVec2 hitMin(mid.x - 12, mid.y - 12);
            ImVec2 hitMax(mid.x + 12, mid.y + 12);
            ImGui::SetCursorScreenPos(hitMin);
            ImGui::InvisibleButton(("trans_" + std::to_string(trans.id)).c_str(), ImVec2(24, 24));
            if (ImGui::IsItemClicked(0)) {
                m_selectedTransitionId = trans.id;
                m_selectedNodeId = 0;
                m_selectionCtx = SelectionContext::Transition;
            }
        }

        // Condition count badge
        if (!trans.conditions.empty()) {
            ImVec2 mid((p1.x + p2.x) * 0.5f, (p1.y + p2.y) * 0.5f);
            char badge[8];
            snprintf(badge, sizeof(badge), "%d", (int)trans.conditions.size());
            dl->AddText(ImVec2(mid.x + 14, mid.y - 14), IM_COL32(200, 200, 100, 255), badge);
        }
    }

    // ── Draw connection wire (while connecting) ──
    const bool connectingActive = m_isConnecting || m_graphConnectDragActive;
    const uint32_t connectFromId = m_isConnecting ? m_connectFromNodeId : m_graphConnectDragFrom;
    if (connectingActive) {
        auto* from = m_stateMachineAsset.FindState(connectFromId);
        if (from) {
            ImVec2 p1 = NodeScreenCenter(*from);
            ImVec2 p2 = ImGui::GetMousePos();
            dl->AddLine(p1, p2, IM_COL32(255, 200, 50, 220), 2.5f);
            // Arrowhead at the cursor end so it reads as a directional connection.
            const ImVec2 dirVec(p2.x - p1.x, p2.y - p1.y);
            const float lenVec = sqrtf(dirVec.x * dirVec.x + dirVec.y * dirVec.y);
            if (lenVec > 1.0f) {
                const float invLen = 1.0f / lenVec;
                const ImVec2 d(dirVec.x * invLen, dirVec.y * invLen);
                const ImVec2 perp(-d.y, d.x);
                const float as = 10.0f;
                dl->AddTriangleFilled(
                    p2,
                    ImVec2(p2.x - d.x * as + perp.x * as * 0.5f, p2.y - d.y * as + perp.y * as * 0.5f),
                    ImVec2(p2.x - d.x * as - perp.x * as * 0.5f, p2.y - d.y * as - perp.y * as * 0.5f),
                    IM_COL32(255, 200, 50, 220));
            }
        }
    }

    // ── Draw state nodes ──
    for (auto& state : m_stateMachineAsset.states) {
        ImVec2 nPos = NodeScreenPos(state);
        ImVec2 nSize(kNodeWidth * m_graphZoom, kNodeHeight * m_graphZoom);
        ImVec2 nEnd(nPos.x + nSize.x, nPos.y + nSize.y);

        bool isSel     = (m_selectedNodeId == state.id);
        bool isDefault = (m_stateMachineAsset.defaultStateId == state.id);

        // Shadow
        dl->AddRectFilled(ImVec2(nPos.x + 3, nPos.y + 3), ImVec2(nEnd.x + 3, nEnd.y + 3),
            IM_COL32(0, 0, 0, 80), 8.0f);

        // Body
        ImU32 nodeCol = StateNodeColor(state.type);
        dl->AddRectFilled(nPos, nEnd, nodeCol, 8.0f);

        // Selection / default border
        if (isSel) dl->AddRect(nPos, nEnd, IM_COL32(255, 255, 255, 255), 8.0f, 0, 3.0f);
        if (isDefault) dl->AddRect(
            ImVec2(nPos.x - 2, nPos.y - 2), ImVec2(nEnd.x + 2, nEnd.y + 2),
            IM_COL32(255, 200, 50, 200), 10.0f, 0, 2.0f);

        // Label — centered, clipped horizontally so long names truncate cleanly.
        const float fontSize = ImGui::GetFontSize();
        const float textW = ImGui::CalcTextSize(state.name.c_str()).x;
        const float availTextW = nSize.x - 16.0f * m_graphZoom;
        const ImVec2 textPos(
            nPos.x + (nSize.x - (std::min)(textW, availTextW)) * 0.5f,
            nPos.y + nSize.y * 0.5f - fontSize * 0.5f);
        dl->PushClipRect(
            ImVec2(nPos.x + 4 * m_graphZoom, nPos.y),
            ImVec2(nEnd.x - 4 * m_graphZoom, nEnd.y),
            true);
        dl->AddText(textPos, IM_COL32(255, 255, 255, 255), state.name.c_str());
        dl->PopClipRect();

        // Type badge (small text)
        const char* typeLabels[] = { "LOCO", "ACT", "DODGE", "JUMP", "DMG", "DEAD", "CUSTOM" };
        int ti = (int)state.type;
        if (ti >= 0 && ti < 7 && m_graphZoom > 0.5f) {
            ImVec2 badgePos(nEnd.x - 40 * m_graphZoom, nPos.y + 2 * m_graphZoom);
            dl->AddText(badgePos, IM_COL32(255, 255, 255, 120), typeLabels[ti]);
        }

        // Output port — small circle on the right edge. Drag from here to
        // create a transition (Unreal-style). Drawn before the node hit area
        // so the port sits visually on top.
        const float portRadius = (std::max)(4.0f, 6.0f * m_graphZoom);
        const ImVec2 portCenter(nEnd.x, nPos.y + nSize.y * 0.5f);
        dl->AddCircleFilled(portCenter, portRadius, IM_COL32(40, 40, 40, 255));
        dl->AddCircleFilled(portCenter, portRadius - 1.5f, IM_COL32(220, 220, 220, 255));

        // Port hit area — sits above the node body, so handle it first.
        ImGui::SetCursorScreenPos(ImVec2(portCenter.x - portRadius - 2.0f, portCenter.y - portRadius - 2.0f));
        const std::string portId = "node_port_" + std::to_string(state.id);
        ImGui::InvisibleButton(portId.c_str(), ImVec2(portRadius * 2.0f + 4.0f, portRadius * 2.0f + 4.0f));
        const bool portHovered = ImGui::IsItemHovered();
        if (portHovered) {
            dl->AddCircle(portCenter, portRadius + 3.0f, IM_COL32(255, 200, 50, 255), 0, 2.0f);
            ImGui::SetTooltip("Drag to another state to create a transition");
        }
        if (ImGui::IsItemActivated()) {
            m_graphConnectDragActive = true;
            m_graphConnectDragFrom = state.id;
        }

        // Node body interaction (positioned after the port so the port wins on overlap).
        ImGui::SetCursorScreenPos(nPos);
        ImGui::InvisibleButton(("node_" + std::to_string(state.id)).c_str(), nSize);
        const bool nodeHovered = ImGui::IsItemHovered();

        if (ImGui::IsItemClicked(0)) {
            if (m_isConnecting) {
                // Finish connection started via right-click "Connect From Here..."
                if (m_connectFromNodeId != state.id) {
                    m_stateMachineAsset.AddTransition(m_connectFromNodeId, state.id);
                    m_stateMachineDirty = true;
                }
                m_isConnecting = false;
                m_connectFromNodeId = 0;
            } else {
                m_selectedNodeId = state.id;
                m_selectedTransitionId = 0;
                m_selectionCtx = SelectionContext::StateNode;
            }
        }

        // Double-click: preview this state using the embedded timeline selection
        if (nodeHovered && ImGui::IsMouseDoubleClicked(0)) {
            PreviewStateNode(state.id, true);
        }

        // Drag node
        if (ImGui::IsItemActive() && ImGui::IsMouseDragging(0) && !m_isConnecting && !m_graphConnectDragActive) {
            ImVec2 delta = ImGui::GetMouseDragDelta(0);
            state.position.x += delta.x / m_graphZoom;
            state.position.y += delta.y / m_graphZoom;
            m_stateMachineDirty = true;
            ImGui::ResetMouseDragDelta();
        }

        // Drop target while a connection drag is active.
        if (m_graphConnectDragActive
            && nodeHovered
            && state.id != m_graphConnectDragFrom
            && ImGui::IsMouseReleased(0)) {
            m_stateMachineAsset.AddTransition(m_graphConnectDragFrom, state.id);
            m_stateMachineDirty = true;
        }

        // Right-click context
        if (ImGui::IsItemClicked(1)) {
            m_selectedNodeId = state.id;
            m_selectionCtx = SelectionContext::StateNode;
            ImGui::OpenPopup("NodeCtx");
        }
    }

    // Cancel connection drag on left release if no node accepted it.
    if (m_graphConnectDragActive && ImGui::IsMouseReleased(0)) {
        m_graphConnectDragActive = false;
        m_graphConnectDragFrom = 0;
    }

    // ── Node context menu ──
    if (ImGui::BeginPopup("NodeCtx")) {
        if (ImGui::MenuItem(ICON_FA_STAR " Set Default")) {
            m_stateMachineAsset.defaultStateId = m_selectedNodeId;
            m_stateMachineDirty = true;
        }
        if (ImGui::MenuItem(ICON_FA_LINK " Connect From Here...")) {
            m_isConnecting = true;
            m_connectFromNodeId = m_selectedNodeId;
        }
        ImGui::Separator();
        if (ImGui::MenuItem(ICON_FA_TRASH " Delete State")) {
            m_stateMachineAsset.RemoveState(m_selectedNodeId);
            m_selectedNodeId = 0;
            m_selectionCtx = SelectionContext::None;
            m_stateMachineDirty = true;
        }
        ImGui::EndPopup();
    }

    // ── Background interaction ──
    ImGui::SetCursorScreenPos(origin);
    ImGui::InvisibleButton("graph_bg", canvasSize);
    const bool bgHovered = ImGui::IsItemHovered();

    // Right-button pan: track press point so a click without movement still
    // opens the context menu. We use IO::MouseDelta for the actual pan because
    // the standard drag-delta tracking gets reset every frame and would lose
    // accumulated movement.
    if (bgHovered && ImGui::IsMouseClicked(1)) {
        m_graphRightPanActive = true;
        const ImVec2 mp = ImGui::GetMousePos();
        m_graphRightPanStart = { mp.x, mp.y };
    }
    if (m_graphRightPanActive && ImGui::IsMouseDown(1)) {
        const ImVec2 frameDelta = ImGui::GetIO().MouseDelta;
        if (frameDelta.x != 0.0f || frameDelta.y != 0.0f) {
            m_graphOffset.x += frameDelta.x;
            m_graphOffset.y += frameDelta.y;
            ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeAll);
        }
    }
    if (ImGui::IsMouseReleased(1) && m_graphRightPanActive) {
        const ImVec2 mp = ImGui::GetMousePos();
        const float totalDx = mp.x - m_graphRightPanStart.x;
        const float totalDy = mp.y - m_graphRightPanStart.y;
        const bool wasClick = (fabsf(totalDx) < 4.0f && fabsf(totalDy) < 4.0f);
        m_graphRightPanActive = false;
        if (wasClick && bgHovered) {
            ImGui::OpenPopup("GraphBgCtx");
        }
    }

    // Middle-button drag also pans, kept for users used to that gesture.
    if (ImGui::IsItemActive() && ImGui::IsMouseDragging(2)) {
        ImVec2 d = ImGui::GetMouseDragDelta(2);
        m_graphOffset.x += d.x;
        m_graphOffset.y += d.y;
        ImGui::ResetMouseDragDelta(2);
    }

    // Zoom with scroll wheel
    if (bgHovered) {
        float wheel = ImGui::GetIO().MouseWheel;
        if (wheel != 0.0f) {
            m_graphZoom *= (wheel > 0) ? 1.1f : 0.9f;
            m_graphZoom = (std::max)(0.2f, (std::min)(4.0f, m_graphZoom));
        }
    }

    if (ImGui::BeginPopup("GraphBgCtx")) {
        ImVec2 mousePos = ImGui::GetMousePosOnOpeningCurrentPopup();
        float posX = (mousePos.x - origin.x - m_graphOffset.x) / m_graphZoom;
        float posY = (mousePos.y - origin.y - m_graphOffset.y) / m_graphZoom;

        ImGui::TextDisabled("Presets:");
        if (ImGui::MenuItem("Setup Full Player"))     ApplyFullPlayerPreset();
        if (ImGui::MenuItem("Setup Locomotion Only")) ApplyLocomotionStateMachinePreset();
        if (ImGui::MenuItem("Add Attack Combo"))      ApplyAttackComboPreset();
        if (ImGui::MenuItem("Add Dodge"))             ApplyDodgePreset();
        if (ImGui::MenuItem("Add Damage"))            ApplyDamagePreset();
        ImGui::Separator();
        ImGui::TextDisabled("Single State:");
        if (ImGui::MenuItem("Locomotion")) AddStateTemplate(StateNodeType::Locomotion, { posX, posY });
        if (ImGui::MenuItem("Action"))     AddStateTemplate(StateNodeType::Action, { posX, posY });
        if (ImGui::MenuItem("Dodge"))      AddStateTemplate(StateNodeType::Dodge, { posX, posY });
        if (ImGui::MenuItem("Jump"))       AddStateTemplate(StateNodeType::Jump, { posX, posY });
        if (ImGui::MenuItem("Damage"))     AddStateTemplate(StateNodeType::Damage, { posX, posY });
        if (ImGui::MenuItem("Dead"))       AddStateTemplate(StateNodeType::Dead, { posX, posY });
        if (ImGui::MenuItem("Custom"))     AddStateTemplate(StateNodeType::Custom, { posX, posY });
        ImGui::EndPopup();
    }

    if (m_stateMachineAsset.states.empty()) {
        const float buttonWidth = 180.0f;
        const float buttonHeight = ImGui::GetFrameHeight();
        const float centerX = origin.x + canvasSize.x * 0.5f - buttonWidth * 0.5f;
        const float centerY = origin.y + canvasSize.y * 0.5f - buttonHeight - 8.0f;

        ImGui::SetCursorScreenPos(ImVec2(centerX, centerY));
        if (ImGui::Button(ICON_FA_PERSON_RUNNING " Setup Full Player##Graph", ImVec2(buttonWidth, 0.0f))) {
            ApplyFullPlayerPreset();
        }

        ImGui::SetCursorScreenPos(ImVec2(centerX, centerY + buttonHeight + 8.0f));
        if (ImGui::Button(ICON_FA_PLUS " Add Locomotion State##Graph", ImVec2(buttonWidth, 0.0f))) {
            AddStateTemplate(StateNodeType::Locomotion, { 0.0f, 0.0f });
        }

        dl->AddText(
            ImVec2(centerX - 8.0f, centerY - 26.0f),
            IM_COL32(180, 180, 180, 255),
            "Right-click graph to add states");
    } else {
        // Persistent gesture cheat-sheet so users do not have to discover the
        // pan / connect / fit shortcuts by trial and error.
        const ImVec2 hintPos(origin.x + 8.0f, origin.y + 8.0f);
        const char* hints[] = {
            "Pan: right-drag    Zoom: wheel    Fit: toolbar button",
            "New transition: drag from a node's right-edge port",
        };
        const ImU32 hintCol = IM_COL32(200, 200, 200, 180);
        for (int i = 0; i < IM_ARRAYSIZE(hints); ++i) {
            const ImVec2 textSize = ImGui::CalcTextSize(hints[i]);
            const ImVec2 boxMin(hintPos.x - 4.0f, hintPos.y + i * 16.0f - 2.0f);
            const ImVec2 boxMax(hintPos.x + textSize.x + 4.0f, hintPos.y + i * 16.0f + textSize.y + 2.0f);
            dl->AddRectFilled(boxMin, boxMax, IM_COL32(0, 0, 0, 110), 3.0f);
            dl->AddText(ImVec2(hintPos.x, hintPos.y + i * 16.0f), hintCol, hints[i]);
        }
    }
}

void PlayerEditorPanel::AddStateTemplate(StateNodeType type, const DirectX::XMFLOAT2& graphPosition)
{
    StateNode* state = m_stateMachineAsset.AddState(GetStateTypeLabel(type), type);
    if (!state) {
        return;
    }

    state->position = graphPosition;
    state->loopAnimation = (type == StateNodeType::Locomotion);
    state->canInterrupt = (type == StateNodeType::Locomotion);
    state->animSpeed = 1.0f;

    if (m_stateMachineAsset.defaultStateId == 0) {
        m_stateMachineAsset.defaultStateId = state->id;
    }

    m_selectedNodeId = state->id;
    m_selectedTransitionId = 0;
    m_selectionCtx = SelectionContext::StateNode;
    m_stateMachineDirty = true;
}

void PlayerEditorPanel::PreviewStateNode(uint32_t stateId, bool restartTimeline)
{
    StateNode* state = m_stateMachineAsset.FindState(stateId);
    if (!state) {
        return;
    }

    m_selectedNodeId = stateId;
    m_selectionCtx = SelectionContext::StateNode;

    if (state->animationIndex >= 0) {
        m_selectedAnimIndex = state->animationIndex;
    }
    (void)restartTimeline;
    RebuildPreviewTimelineRuntimeData();

    StartSelectedAnimationPreview();
    if (m_previewState.IsActive()) {
        m_previewState.SetLoop(state->loopAnimation);
    }
    if (CanUsePreviewEntity()) {
        if (PlaybackComponent* playback = m_registry->GetComponent<PlaybackComponent>(m_previewEntity)) {
            playback->looping = state->loopAnimation;
        }
    }
}

// ----------------------------------------------------------------------------
// Inspector panes (state node, parameter list, transition condition editor)
// ----------------------------------------------------------------------------

void PlayerEditorPanel::DrawStateNodeInspector()
{
    auto* state = m_stateMachineAsset.FindState(m_selectedNodeId);
    if (!state) { ImGui::TextDisabled("No state selected"); return; }

    ImGui::Text(ICON_FA_CIRCLE_NODES " State: %s", state->name.c_str());
    if (m_stateMachineAsset.defaultStateId == state->id) {
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.2f, 1.0f), "[Default]");
    }
    ImGui::Separator();

    char nameBuf[128];
    strncpy_s(nameBuf, state->name.c_str(), _TRUNCATE);
    if (ImGui::InputText("Name", nameBuf, sizeof(nameBuf))) { state->name = nameBuf; m_stateMachineDirty = true; }

    int typeInt = static_cast<int>(state->type);
    const char* typeNames[] = { "Locomotion", "Action", "Dodge", "Jump", "Damage", "Dead", "Custom" };
    if (ImGui::Combo("Type", &typeInt, typeNames, IM_ARRAYSIZE(typeNames))) {
        state->type = static_cast<StateNodeType>(typeInt);
        m_stateMachineDirty = true;
    }

    ImGui::Separator();
    ImGui::Text("Animation");
    if (DrawAnimationSelector("Animation", &state->animationIndex)) m_stateMachineDirty = true;
    if (ImGui::Checkbox("Loop", &state->loopAnimation)) m_stateMachineDirty = true;
    if (ImGui::DragFloat("Speed", &state->animSpeed, 0.01f, 0.0f, 5.0f, "%.2f")) m_stateMachineDirty = true;
    if (ImGui::Checkbox("Can Interrupt", &state->canInterrupt)) m_stateMachineDirty = true;
    if (ImGui::Button(ICON_FA_PLAY " Preview State")) {
        PreviewStateNode(state->id, false);
    }
    ImGui::SameLine();
    if (ImGui::Button(ICON_FA_ROTATE_LEFT " Preview From Start")) {
        PreviewStateNode(state->id, true);
    }

    ImGui::Text("Input Map");
    const auto& editingMap = m_inputMappingTab.GetEditingMap();
    ImGui::TextDisabled("Action Map: %s", editingMap.name.empty() ? "(none)" : editingMap.name.c_str());
    ImGui::TextDisabled("Actions: %d / Axes: %d", static_cast<int>(editingMap.actions.size()), static_cast<int>(editingMap.axes.size()));
    ImGui::Separator();
    ImGui::Text("Outgoing Transitions");
    auto transitions = m_stateMachineAsset.GetTransitionsFrom(m_selectedNodeId);
    for (auto* t : transitions) {
        auto* to = m_stateMachineAsset.FindState(t->toState);
        std::string label = to ? (ICON_FA_ARROW_RIGHT " " + to->name) : "-> ???";
        if (ImGui::Selectable(label.c_str(), m_selectedTransitionId == t->id)) {
            m_selectedTransitionId = t->id;
            m_selectionCtx = SelectionContext::Transition;
        }
    }
}

void PlayerEditorPanel::DrawStateMachineParameterList()
{
    ImGui::Text(ICON_FA_SLIDERS " Parameters");
    ImGui::Separator();

    if (ImGui::Button(ICON_FA_PLUS " Add Move Params")) {
        EnsureStateMachineParameter("MoveX", ParameterType::Float, 0.0f);
        EnsureStateMachineParameter("MoveY", ParameterType::Float, 0.0f);
        EnsureStateMachineParameter("MoveMagnitude", ParameterType::Float, 0.0f);
        EnsureStateMachineParameter("IsMoving", ParameterType::Bool, 0.0f);
        EnsureStateMachineParameter("Gait", ParameterType::Int, 0.0f);
        EnsureStateMachineParameter("IsWalking", ParameterType::Bool, 0.0f);
        EnsureStateMachineParameter("IsRunning", ParameterType::Bool, 0.0f);
        m_stateMachineDirty = true;
    }

    ImGui::SameLine();
    if (ImGui::Button(ICON_FA_BOLT " Add Attack Combo")) {
        ApplyAttackComboPreset();
    }

    ImGui::Separator();
    DrawStateMachineRuntimeStatus();
    ImGui::Separator();

    for (int i = 0; i < static_cast<int>(m_stateMachineAsset.parameters.size()); ++i) {
        auto& parameter = m_stateMachineAsset.parameters[i];
        ImGui::PushID(i);

        const StateMachineParamsComponent* runtimeParams =
            CanUsePreviewEntity() ? m_registry->GetComponent<StateMachineParamsComponent>(m_previewEntity) : nullptr;

        char nameBuf[64];
        strncpy_s(nameBuf, parameter.name.c_str(), _TRUNCATE);
        if (ImGui::InputText("Name", nameBuf, sizeof(nameBuf))) {
            parameter.name = nameBuf;
            m_stateMachineDirty = true;
        }

        int typeInt = static_cast<int>(parameter.type);
        const char* typeNames[] = { "Float", "Int", "Bool", "Trigger" };
        if (ImGui::Combo("Type", &typeInt, typeNames, IM_ARRAYSIZE(typeNames))) {
            parameter.type = static_cast<ParameterType>(typeInt);
            m_stateMachineDirty = true;
        }

        if (ImGui::DragFloat("Default", &parameter.defaultValue, 0.1f)) {
            m_stateMachineDirty = true;
        }

        if (runtimeParams) {
            ImGui::TextDisabled("Runtime: %.3f", runtimeParams->GetParam(parameter.name.c_str()));
        }

        if (ImGui::Button(ICON_FA_TRASH " Remove")) {
            m_stateMachineAsset.parameters.erase(m_stateMachineAsset.parameters.begin() + i);
            m_stateMachineDirty = true;
            ImGui::PopID();
            break;
        }

        ImGui::Separator();
        ImGui::PopID();
    }

    if (ImGui::Button(ICON_FA_PLUS " Add Parameter")) {
        ParameterDef parameter;
        parameter.name = "Param";
        m_stateMachineAsset.parameters.push_back(std::move(parameter));
        m_stateMachineDirty = true;
    }
}

void PlayerEditorPanel::DrawTransitionConditionEditor(StateTransition* trans)
{
    auto* from = m_stateMachineAsset.FindState(trans->fromState);
    auto* to   = m_stateMachineAsset.FindState(trans->toState);

    ImGui::Text(ICON_FA_ARROW_RIGHT " Transition");
    ImGui::Text("%s -> %s", from ? from->name.c_str() : "?", to ? to->name.c_str() : "?");
    ImGui::Separator();

    if (ImGui::DragInt("Priority", &trans->priority, 1, 0, 100)) m_stateMachineDirty = true;
    if (ImGui::Checkbox("Has Exit Time", &trans->hasExitTime)) m_stateMachineDirty = true;
    if (trans->hasExitTime) {
        if (ImGui::DragFloat("Exit Time (0-1)", &trans->exitTimeNormalized, 0.01f, 0.0f, 1.0f)) m_stateMachineDirty = true;
    }
    if (ImGui::DragFloat("Blend Duration", &trans->blendDuration, 0.01f, 0.0f, 2.0f)) m_stateMachineDirty = true;

    ImGui::Separator();
    ImGui::TextDisabled("Summary");
    for (int ci = 0; ci < (int)trans->conditions.size(); ++ci) {
        const auto& cond = trans->conditions[ci];
        std::string summary = std::string(ResolveConditionTypeLabel(cond.type)) + " ";
        if (cond.type == ConditionType::Input || cond.type == ConditionType::Parameter) {
            summary += (cond.param[0] != '\0') ? cond.param : "(unset)";
            summary += " ";
        }
        summary += ResolveCompareOpLabel(cond.compare);
        summary += " ";
        if (cond.type == ConditionType::AnimEnd) {
            summary += (cond.value != 0.0f) ? "true" : "false";
        } else {
            summary += std::to_string(cond.value);
        }
        ImGui::BulletText("%s", summary.c_str());
    }
    if (trans->conditions.empty()) {
        ImGui::TextDisabled("No conditions.");
    }
    ImGui::Separator();
    if (ImGui::Button(ICON_FA_PERSON_RUNNING " Use IsMoving >= 1")) {
        ApplyLocomotionTransitionPreset(*trans, true);
        m_stateMachineDirty = true;
    }
    ImGui::SameLine();
    if (ImGui::Button(ICON_FA_PERSON_RUNNING " Use IsMoving <= 0")) {
        ApplyLocomotionTransitionPreset(*trans, false);
        m_stateMachineDirty = true;
    }

    ImGui::Separator();
    ImGui::Text("Conditions (%d):", (int)trans->conditions.size());

    for (int ci = 0; ci < (int)trans->conditions.size(); ++ci) {
        auto& cond = trans->conditions[ci];
        ImGui::PushID(ci);

        int typeInt = static_cast<int>(cond.type);
        const char* condTypes[] = { "Input", "Timer", "AnimEnd", "Health", "Stamina", "Parameter" };
        ImGui::SetNextItemWidth(90);
        if (ImGui::Combo("##T", &typeInt, condTypes, IM_ARRAYSIZE(condTypes))) {
            cond.type = static_cast<ConditionType>(typeInt);
            m_stateMachineDirty = true;
        }

        ImGui::SameLine();
        ImGui::SetNextItemWidth(140);
        if (cond.type == ConditionType::Input && !m_inputMappingTab.GetEditingMap().actions.empty()) {
            const auto& actions = m_inputMappingTab.GetEditingMap().actions;
            const char* preview = cond.param[0] != '\0' ? cond.param : "(select action)";
            if (ImGui::BeginCombo("##P", preview)) {
                for (const auto& action : actions) {
                    const bool selected = strcmp(cond.param, action.actionName.c_str()) == 0;
                    if (ImGui::Selectable(action.actionName.c_str(), selected)) {
                        strncpy_s(cond.param, action.actionName.c_str(), _TRUNCATE);
                        m_stateMachineDirty = true;
                    }
                    if (selected) {
                        ImGui::SetItemDefaultFocus();
                    }
                }
                ImGui::EndCombo();
            }
        } else if (ImGui::InputText("##P", cond.param, sizeof(cond.param))) {
            m_stateMachineDirty = true;
        }

        ImGui::SameLine();
        int cmpInt = static_cast<int>(cond.compare);
        const char* cmpOps[] = { "==", "!=", ">", "<", ">=", "<=" };
        ImGui::SetNextItemWidth(45);
        if (ImGui::Combo("##C", &cmpInt, cmpOps, IM_ARRAYSIZE(cmpOps))) {
            cond.compare = static_cast<CompareOp>(cmpInt);
            m_stateMachineDirty = true;
        }

        ImGui::SameLine();
        ImGui::SetNextItemWidth(55);
        if (ImGui::DragFloat("##V", &cond.value, 0.1f)) m_stateMachineDirty = true;

        ImGui::SameLine();
        if (ImGui::Button(ICON_FA_XMARK)) {
            trans->conditions.erase(trans->conditions.begin() + ci);
            m_stateMachineDirty = true;
            ImGui::PopID();
            break;
        }

        ImGui::PopID();
    }

    if (ImGui::Button(ICON_FA_PLUS " Condition")) {
        trans->conditions.push_back(TransitionCondition{});
        m_stateMachineDirty = true;
    }

    ImGui::Separator();
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.6f, 0.15f, 0.15f, 1.0f));
    if (ImGui::Button(ICON_FA_TRASH " Delete Transition")) {
        m_stateMachineAsset.RemoveTransition(m_selectedTransitionId);
        m_selectedTransitionId = 0;
        m_selectionCtx = SelectionContext::None;
        m_stateMachineDirty = true;
    }
    ImGui::PopStyleColor();
}
