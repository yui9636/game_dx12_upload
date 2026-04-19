#pragma once
#include <string>
#include <vector>
#include <cstdint>
#include <unordered_map>
#include <DirectXMath.h>

// ============================================================================
// State Types
// ============================================================================

enum class StateNodeType : uint8_t
{
    Locomotion = 0,
    Action,
    Dodge,
    Jump,
    Damage,
    Dead,
    Custom
};

// ============================================================================
// Transition Condition
// ============================================================================

enum class ConditionType : uint8_t
{
    Input = 0,     // Action button pressed/held/released
    Timer,         // Time elapsed in state
    AnimEnd,       // Animation finished
    Health,        // Health threshold
    Stamina,       // Stamina threshold
    Parameter,     // Custom parameter check
};

enum class CompareOp : uint8_t
{
    Equal = 0, NotEqual, Greater, Less, GreaterEqual, LessEqual
};

struct TransitionCondition
{
    ConditionType type    = ConditionType::Input;
    char          param[64] = {};   // Action name, parameter name, etc.
    CompareOp     compare = CompareOp::Equal;
    float         value   = 0.0f;
};

// ============================================================================
// State Transition
// ============================================================================

struct StateTransition
{
    uint32_t id       = 0;
    uint32_t fromState = 0;
    uint32_t toState   = 0;
    int      priority  = 0;

    std::vector<TransitionCondition> conditions;

    float exitTimeNormalized = 0.0f;  // 0=immediate, 0.9=at 90% of anim
    float blendDuration      = 0.2f;  // Transition blend in seconds
    bool  hasExitTime        = false; // Must wait for exitTimeNormalized
};

// ============================================================================
// State Node
// ============================================================================

struct StateNode
{
    uint32_t      id   = 0;
    std::string   name;
    StateNodeType type = StateNodeType::Locomotion;

    int           animationIndex = -1;
    uint32_t      timelineId      = 0;
    bool          loopAnimation  = false;
    float         animSpeed      = 1.0f;
    bool          canInterrupt   = true;

    // Editor layout position
    DirectX::XMFLOAT2 position{ 0, 0 };

    // Custom properties (name -> value)
    std::unordered_map<std::string, float> properties;
};

// ============================================================================
// Parameter Definition
// ============================================================================

enum class ParameterType : uint8_t
{
    Float = 0, Int, Bool, Trigger
};

struct ParameterDef
{
    std::string   name;
    ParameterType type         = ParameterType::Float;
    float         defaultValue = 0.0f;
};

// ============================================================================
// State Machine Asset (top-level document)
// ============================================================================

struct StateMachineAsset
{
    std::string name;

    std::vector<StateNode>       states;
    std::vector<StateTransition> transitions;
    std::vector<ParameterDef>    parameters;

    uint32_t defaultStateId = 0;
    uint32_t nextId         = 1;

    // Helpers
    uint32_t GenerateId() { return nextId++; }

    StateNode* AddState(const std::string& stateName, StateNodeType type)
    {
        StateNode s;
        s.id   = GenerateId();
        s.name = stateName;
        s.type = type;
        states.push_back(std::move(s));
        return &states.back();
    }

    StateTransition* AddTransition(uint32_t from, uint32_t to)
    {
        StateTransition t;
        t.id        = GenerateId();
        t.fromState = from;
        t.toState   = to;
        transitions.push_back(std::move(t));
        return &transitions.back();
    }

    StateNode* FindState(uint32_t id)
    {
        for (auto& s : states) if (s.id == id) return &s;
        return nullptr;
    }

    const StateNode* FindState(uint32_t id) const
    {
        for (auto& s : states) if (s.id == id) return &s;
        return nullptr;
    }

    void RemoveState(uint32_t id)
    {
        states.erase(
            std::remove_if(states.begin(), states.end(),
                [id](const StateNode& s) { return s.id == id; }),
            states.end()
        );
        // Remove related transitions
        transitions.erase(
            std::remove_if(transitions.begin(), transitions.end(),
                [id](const StateTransition& t) { return t.fromState == id || t.toState == id; }),
            transitions.end()
        );
    }

    void RemoveTransition(uint32_t id)
    {
        transitions.erase(
            std::remove_if(transitions.begin(), transitions.end(),
                [id](const StateTransition& t) { return t.id == id; }),
            transitions.end()
        );
    }

    // Get all transitions from a given state
    std::vector<const StateTransition*> GetTransitionsFrom(uint32_t stateId) const
    {
        std::vector<const StateTransition*> result;
        for (auto& t : transitions)
            if (t.fromState == stateId) result.push_back(&t);
        return result;
    }
};
