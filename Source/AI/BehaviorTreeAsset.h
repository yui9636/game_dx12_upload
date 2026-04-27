#pragma once
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

#include <DirectXMath.h>

#include "BlackboardComponent.h"

// Behavior Tree node category (used by validate / UI grouping).
enum class BTNodeCategory : uint8_t
{
    Root      = 0,
    Composite = 1,
    Decorator = 2,
    Action    = 3,
    Condition = 4,
};

// Concrete node types.
// Numbering uses 100-stride blocks per category to leave room for additions.
enum class BTNodeType : uint16_t
{
    Root              = 0,

    // Composite (100s)
    Sequence          = 100,
    Selector          = 101,
    Parallel          = 102,

    // Decorator (200s)
    Inverter          = 200,
    Repeat            = 201,
    Cooldown          = 202,
    ConditionGuard    = 203,

    // Condition (300s)
    HasTarget         = 300,
    TargetInRange     = 301,
    TargetVisible     = 302,
    HealthBelow       = 303,
    StaminaAbove      = 304,
    BlackboardEqual   = 305,

    // Action: locomotion / movement (400s)
    Wait              = 400,
    FaceTarget        = 401,
    MoveToTarget      = 402,
    StrafeAroundTarget= 403,
    Retreat           = 404,

    // Action: combat (500s)
    // NOTE: per PlayerEditor_SetupFullPlayer_Spec section 3, attacks are
    //       unified into a single "Attack" channel. AttackHeavy is intentionally absent.
    Attack            = 500,
    DodgeAction       = 502,

    // Action: state-machine I/F (600s)
    SetSMParam        = 600,
    PlayState         = 601,

    // Action: blackboard (700s)
    SetBlackboard     = 700,
};

// Single tree node (data). All node-type specific knobs live in fParam* / iParam* / sParam*.
struct BTNode
{
    uint32_t          id    = 0;
    BTNodeType        type  = BTNodeType::Root;
    std::string       name;
    std::vector<uint32_t> childrenIds;

    float       fParam0 = 0.0f;
    float       fParam1 = 0.0f;
    float       fParam2 = 0.0f;
    int         iParam0 = 0;
    std::string sParam0;
    std::string sParam1;
    BlackboardValueType bbType = BlackboardValueType::None;

    DirectX::XMFLOAT2 graphPos { 0.0f, 0.0f };
};

// Authoring data for an AI behavior tree.
struct BehaviorTreeAsset
{
    int                  version = 1;
    uint32_t             rootId  = 0;
    std::vector<BTNode>  nodes;

    const BTNode* FindNode(uint32_t id) const;
    BTNode*       FindNode(uint32_t id);
    uint32_t      AllocateNodeId() const;

    static BehaviorTreeAsset CreateAggressiveTemplate();
    static BehaviorTreeAsset CreateDefensiveTemplate();
    static BehaviorTreeAsset CreatePatrolTemplate();

    bool LoadFromFile(const std::filesystem::path& path);
    bool SaveToFile(const std::filesystem::path& path) const;
};

// Validate severity (matches GameLoop validate severity for editor reuse).
enum class BTValidateSeverity : uint8_t
{
    Info    = 0,
    Warning = 1,
    Error   = 2,
};

struct BTValidateMessage
{
    BTValidateSeverity severity = BTValidateSeverity::Info;
    std::string        message;
};

struct BTValidateResult
{
    std::vector<BTValidateMessage> messages;

    bool HasError() const;
    int  ErrorCount() const;
    int  WarningCount() const;
};

BTValidateResult ValidateBehaviorTree(const BehaviorTreeAsset& asset);

// Helpers (string conversions used by JSON serializer and editor UI).
const char*    BTNodeTypeToString(BTNodeType t);
BTNodeType     BTNodeTypeFromString(const std::string& s);
BTNodeCategory CategoryOfBTNodeType(BTNodeType t);
