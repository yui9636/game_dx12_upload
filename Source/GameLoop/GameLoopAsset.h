#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include <filesystem>

#include "Component/ActorTypeComponent.h"

// GameLoop で扱う遷移条件の種別。
enum class GameLoopConditionType : uint8_t
{
    None              = 0,
    InputPressed      = 1,
    UIButtonClicked   = 2,
    TimerElapsed      = 3,
    ActorDead         = 4,
    AllActorsDead     = 5,
    ActorMovedDistance= 6,
    RuntimeFlag       = 7,

    // Phase 4 拡張。
    StateMachineState = 100,
    TimelineEvent     = 101,
    CustomEvent       = 102,
};

// 1 つの transition 成立条件。
struct GameLoopCondition
{
    GameLoopConditionType type = GameLoopConditionType::None;

    ActorType    actorType     = ActorType::None;
    std::string  targetName;     // UIButtonClicked の buttonId / 将来の actor name
    std::string  parameterName;  // RuntimeFlag のキー / StateMachineState のステート名
    std::string  eventName;      // TimelineEvent / CustomEvent のイベント名
    int          actionIndex    = -1;
    float        threshold      = 0.0f;
    float        seconds        = 0.0f;
};

// scene を表す node の型。
enum class GameLoopNodeType : uint8_t
{
    Scene = 0,
};

// GameLoop graph の 1 node = 1 scene。
struct GameLoopNode
{
    uint32_t          id        = 0;
    std::string       name;
    std::string       scenePath;
    GameLoopNodeType  type      = GameLoopNodeType::Scene;
};

// node から node への遷移。
struct GameLoopTransition
{
    uint32_t                       fromNodeId           = 0;
    uint32_t                       toNodeId             = 0;
    std::string                    name;
    std::vector<GameLoopCondition> conditions;
    bool                           requireAllConditions = true;
};

// scene 進行 graph の authoring data。
struct GameLoopAsset
{
    int                              version     = 1;
    uint32_t                         startNodeId = 0;
    std::vector<GameLoopNode>        nodes;
    std::vector<GameLoopTransition>  transitions;

    // GameLoopNode を id から検索する。なければ nullptr。
    const GameLoopNode* FindNode(uint32_t id) const;
    GameLoopNode*       FindNode(uint32_t id);

    // 新規 node の id を採番する (現在の最大 id + 1)。
    uint32_t AllocateNodeId() const;

    // 標準サンプル loop (Title -> Battle -> Result -> Retry / Cancel) を生成する。
    static GameLoopAsset CreateDefault();

    // JSON ファイルから読み込む (Phase 2)。
    bool LoadFromFile(const std::filesystem::path& path);

    // JSON ファイルに保存する (Phase 2)。
    bool SaveToFile(const std::filesystem::path& path) const;
};

// validate 結果の severity。
enum class GameLoopValidateSeverity : uint8_t
{
    Info    = 0,
    Warning = 1,
    Error   = 2,
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

// asset を validate する。
GameLoopValidateResult ValidateGameLoopAsset(const GameLoopAsset& asset);
