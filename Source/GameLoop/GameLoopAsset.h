#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

#include <DirectXMath.h>

struct GameLoopTransitionInput
{
    uint32_t keyboardScancode = 0;
    uint8_t gamepadButton = 0xFF;
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

enum class GameLoopNodeType : uint8_t
{
    Scene = 0,
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
    GameLoopTransitionInput input;
    GameLoopLoadingPolicy loadingPolicy;
};

struct GameLoopAsset
{
    int version = 4;
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
