#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

#include <DirectXMath.h>

constexpr int kGameFlowAssetVersion = 1;
constexpr uint8_t kGameFlowUnboundGamepadButton = 0xFF;

enum class GameLoopNodeType : uint8_t
{
    Scene = 0,
    Flow = 1,
};

enum class GameFlowConditionMode : uint8_t
{
    All = 0,
    Any = 1,
};

enum class GameFlowConditionType : uint8_t
{
    Event = 0,
    InputAction = 1,
    UIButtonClick = 2,
    TimerElapsed = 3,
    FlagEquals = 4,
    BattleResult = 5,
    SceneLoaded = 6,
};

enum class GameFlowActionType : uint8_t
{
    LoadScene = 0,
    SetCurrentNode = 1,
    EmitEvent = 2,
    SetFlag = 3,
    ClearFlag = 4,
    StartBattleFlow = 5,
    ResetBattleFlow = 6,
    Fade = 7,
    Wait = 8,
};

struct GameFlowCondition
{
    GameFlowConditionType type = GameFlowConditionType::UIButtonClick;
    std::string name;
    std::string value;
    uint32_t keyboardScancode = 0;
    uint8_t gamepadButton = kGameFlowUnboundGamepadButton;
    float seconds = 0.0f;
    bool expectedFlagValue = true;
};

struct GameFlowAction
{
    GameFlowActionType type = GameFlowActionType::LoadScene;
    std::string target;
    std::string value;
    bool boolValue = true;
    float seconds = 0.0f;
};

struct GameLoopNode
{
    uint32_t id = 0;
    std::string name;
    std::string scenePath;
    GameLoopNodeType type = GameLoopNodeType::Scene;
    DirectX::XMFLOAT2 graphPos = { 0.0f, 0.0f };
};

struct GameLoopTransition
{
    uint32_t id = 0;
    uint32_t fromNodeId = 0;
    uint32_t toNodeId = 0;
    std::string name;
    int priority = 0;
    GameFlowConditionMode conditionMode = GameFlowConditionMode::All;
    std::vector<GameFlowCondition> conditions;
    std::vector<GameFlowAction> actions;
};

struct GameLoopAsset
{
    int version = kGameFlowAssetVersion;
    uint32_t startNodeId = 0;
    uint32_t nextNodeId = 1;
    uint32_t nextTransitionId = 1;
    std::vector<GameLoopNode> nodes;
    std::vector<GameLoopTransition> transitions;

    const GameLoopNode* FindNode(uint32_t id) const;
    GameLoopNode* FindNode(uint32_t id);

    uint32_t AllocateNodeId();
    uint32_t AllocateTransitionId();

    static GameLoopAsset CreateDefault();
    static GameLoopAsset CreateZTestLoop();

    bool LoadFromFile(const std::filesystem::path& path);
    bool SaveToFile(const std::filesystem::path& path) const;
};

enum class GameLoopValidateSeverity : uint8_t
{
    Info = 0,
    Warning = 1,
    Error = 2,
};

struct GameLoopValidateMessage
{
    GameLoopValidateSeverity severity = GameLoopValidateSeverity::Info;
    std::string message;
};

struct GameLoopValidateResult
{
    std::vector<GameLoopValidateMessage> messages;

    bool HasError() const;
    int ErrorCount() const;
    int WarningCount() const;
};

GameLoopValidateResult ValidateGameLoopAsset(const GameLoopAsset& asset);
std::string NormalizeGameLoopScenePath(const std::string& path);

using GameFlowAsset = GameLoopAsset;
using GameFlowNode = GameLoopNode;
using GameFlowTransition = GameLoopTransition;
using GameFlowValidateResult = GameLoopValidateResult;
