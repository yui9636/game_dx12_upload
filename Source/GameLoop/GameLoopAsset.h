#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include <filesystem>
#include <DirectXMath.h>

#include "Component/ActorTypeComponent.h"

// Condition kind used by GameLoop transitions.
enum class GameLoopConditionType : uint8_t
{
    None = 0,
    InputPressed = 1,
    UIButtonClicked = 2,
    TimerElapsed = 3,
    ActorDead = 4,
    AllActorsDead = 5,
    ActorMovedDistance = 6,
    RuntimeFlag = 7,

    // Phase 4 extensions.
    StateMachineState = 100,
    TimelineEvent = 101,
    CustomEvent = 102,
};

// One transition condition.
struct GameLoopCondition
{
    GameLoopConditionType type = GameLoopConditionType::None;

    ActorType    actorType = ActorType::None;
    std::string  targetName;     // UIButtonClicked buttonId / future actor name.
    std::string  parameterName;  // RuntimeFlag key / StateMachineState state name.
    std::string  eventName;      // TimelineEvent / CustomEvent name.
    int          actionIndex = -1;
    float        threshold = 0.0f;
    float        seconds = 0.0f;
};

enum class GameLoopLoadingMode : uint8_t
{
    Immediate = 0,
    FadeOnly,
    LoadingOverlay,
};

struct GameLoopLoadingPolicy
{
    GameLoopLoadingMode mode = GameLoopLoadingMode::Immediate;
    float fadeOutSeconds = 0.15f;
    float fadeInSeconds = 0.15f;
    float minimumLoadingSeconds = 0.0f;
    std::string loadingMessage;
    bool blockInput = true;
};

// Node kind. Phase 1 only supports Scene.
enum class GameLoopNodeType : uint8_t
{
    Scene = 0,
};

// One node in the GameLoop graph (one node = one scene).
struct GameLoopNode
{
    uint32_t          id = 0;
    std::string       name;
    std::string       scenePath;
    GameLoopNodeType  type = GameLoopNodeType::Scene;

    // Editor graph position. This is authoring-only data and is saved into .gameloop.
    DirectX::XMFLOAT2 graphPos = { 0.0f, 0.0f };
};

// Transition between two nodes.
struct GameLoopTransition
{
    uint32_t                       id = 0;
    uint32_t                       fromNodeId = 0;
    uint32_t                       toNodeId = 0;
    std::string                    name;
    std::vector<GameLoopCondition> conditions;
    bool                           requireAllConditions = true;
    GameLoopLoadingPolicy          loadingPolicy;
};

// Authoring data for the scene transition graph.
struct GameLoopAsset
{
    int                              version = 2;
    uint32_t                         startNodeId = 0;
    uint32_t                         nextNodeId = 1;
    uint32_t                         nextTransitionId = 1;
    std::vector<GameLoopNode>        nodes;
    std::vector<GameLoopTransition>  transitions;

    // Returns nullptr if id is not present.
    const GameLoopNode* FindNode(uint32_t id) const;
    GameLoopNode* FindNode(uint32_t id);

    // Allocate a fresh node id (max+1).
    uint32_t AllocateNodeId();
    uint32_t AllocateTransitionId();

    // Build the standard sample loop (Title -> Battle -> Result, with Retry / Cancel).
    static GameLoopAsset CreateDefault();

    // Build the simplest Z/Confirm test loop (Title -> Battle -> Result -> Battle).
    static GameLoopAsset CreateZTestLoop();

    // JSON load / save.
    bool LoadFromFile(const std::filesystem::path& path);
    bool SaveToFile(const std::filesystem::path& path) const;
};

// Severity of a validate message.
enum class GameLoopValidateSeverity : uint8_t
{
    Info = 0,
    Warning = 1,
    Error = 2,
};

struct GameLoopValidateMessage
{
    GameLoopValidateSeverity severity = GameLoopValidateSeverity::Info;
    std::string              message;
};

struct GameLoopValidateResult
{
    std::vector<GameLoopValidateMessage> messages;

    bool HasError() const;
    int  ErrorCount() const;
    int  WarningCount() const;
};

// Run all validate rules against the asset.
GameLoopValidateResult ValidateGameLoopAsset(const GameLoopAsset& asset);

// Stores scene paths in a portable Data/... form. Returns empty if the path
// cannot be normalized into the project Data directory.
std::string NormalizeGameLoopScenePath(const std::string& path);
